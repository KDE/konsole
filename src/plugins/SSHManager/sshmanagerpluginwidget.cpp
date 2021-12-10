/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagerpluginwidget.h"

#include "konsoledebug.h"
#include "session/SessionController.h"
#include "sshmanagermodel.h"
#include "terminalDisplay/TerminalDisplay.h"

#include "profile/ProfileModel.h"

#include "sshmanagerfiltermodel.h"
#include "ui_sshwidget.h"

#include <KLocalizedString>

#include <QAction>
#include <QDebug>
#include <QIntValidator>
#include <QItemSelectionModel>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSortFilterProxyModel>

struct SSHManagerTreeWidget::Private {
    SSHManagerModel *model = nullptr;
    SSHManagerFilterModel *filterModel = nullptr;
    Konsole::SessionController *controller = nullptr;
};

SSHManagerTreeWidget::SSHManagerTreeWidget(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::SSHTreeWidget>())
    , d(std::make_unique<SSHManagerTreeWidget::Private>())
{
    ui->setupUi(this);
    ui->errorPanel->hide();

    d->filterModel = new SSHManagerFilterModel(this);

    // https://stackoverflow.com/questions/1418423/the-hostname-regex
    const auto hostnameRegex =
        QRegularExpression(QStringLiteral(R"(^[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$)"));

    const auto *hostnameValidator = new QRegularExpressionValidator(hostnameRegex, this);
    ui->hostname->setValidator(hostnameValidator);

    // System and User ports see:
    // https://www.iana.org/assignments/service-names-port-numbers/service-names-port-numbers.xhtml
    const auto *portValidator = new QIntValidator(0, 49151, this);
    ui->port->setValidator(portValidator);

    connect(ui->newSSHConfig, &QPushButton::clicked, this, &SSHManagerTreeWidget::showInfoPane);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SSHManagerTreeWidget::clearSshInfo);
    connect(ui->btnEdit, &QPushButton::clicked, this, &SSHManagerTreeWidget::editSshInfo);
    connect(ui->btnImport, &QPushButton::clicked, this, &SSHManagerTreeWidget::requestImport);
    connect(ui->btnRemove, &QPushButton::clicked, this, &SSHManagerTreeWidget::triggerRemove);
    connect(ui->btnInvertFilter, &QPushButton::clicked, d->filterModel, &SSHManagerFilterModel::setInvertFilter);

    connect(ui->filterText, &QLineEdit::textChanged, this, [this] {
        d->filterModel->setFilterRegularExpression(ui->filterText->text());
        d->filterModel->invalidate();
    });

    ui->profile->setModel(Konsole::ProfileModel::instance());

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeView, &QTreeView::customContextMenuRequested, [this](const QPoint &pos) {
        if (!ui->treeView->indexAt(pos).isValid()) {
            return;
        }

        QMenu *menu = new QMenu(this);
        auto action = new QAction(QStringLiteral("Remove"), ui->treeView);
        menu->addAction(action);

        connect(action, &QAction::triggered, this, &SSHManagerTreeWidget::triggerRemove);

        menu->popup(ui->treeView->viewport()->mapToGlobal(pos));
    });

    connect(ui->treeView, &QTreeView::doubleClicked, this, &SSHManagerTreeWidget::connectRequested);
    connect(ui->treeView, &SshTreeView::mouseButtonClicked, this, &SSHManagerTreeWidget::handleTreeClick);

    ui->treeView->setModel(d->filterModel);

    clearSshInfo();
}

SSHManagerTreeWidget::~SSHManagerTreeWidget() = default;

void SSHManagerTreeWidget::addSshInfo()
{
    SSHConfigurationData data;
    auto [error, errorString] = checkFields();
    if (error) {
        ui->errorPanel->setText(errorString);
        ui->errorPanel->show();
        return;
    }

    d->model->addChildItem(info(), ui->folder->currentText());
    clearSshInfo();
}

void SSHManagerTreeWidget::saveEdit()
{
    SSHConfigurationData data;
    auto [error, errorString] = checkFields();
    if (error) {
        ui->errorPanel->setText(errorString);
        ui->errorPanel->show();
        return;
    }

    auto selection = ui->treeView->selectionModel()->selectedIndexes();
    auto sourceIdx = d->filterModel->mapToSource(selection.at(0));
    d->model->editChildItem(info(), sourceIdx);

    clearSshInfo();
}

void SSHManagerTreeWidget::requestImport()
{
    d->model->startImportFromSshConfig();
}

SSHConfigurationData SSHManagerTreeWidget::info() const
{
    SSHConfigurationData data;
    data.host = ui->hostname->text().trimmed();
    data.name = ui->name->text().trimmed();
    data.port = ui->port->text().trimmed();
    data.sshKey = ui->sshkey->text().trimmed();
    data.profileName = ui->profile->currentText().trimmed();
    data.username = ui->username->text().trimmed();
    data.useSshConfig = ui->useSshConfig->checkState() == Qt::Checked;
    return data;
}

void SSHManagerTreeWidget::triggerRemove()
{
    auto selection = ui->treeView->selectionModel()->selectedIndexes();
    if (selection.empty()) {
        return;
    }

    const QString text = selection.at(0).data(Qt::DisplayRole).toString();
    const QString dialogMessage = ui->treeView->model()->rowCount(selection.at(0))
        ? i18n("You are about to remove the folder %1,\n with multiple SSH Configurations, are you sure?", text)
        : i18n("You are about to remove %1, are you sure?", text);

    int result = QMessageBox::warning(this, i18n("Remove SSH Configurations"), dialogMessage, QMessageBox::Ok, QMessageBox::Cancel);

    if (result == QMessageBox::Cancel) {
        return;
    }

    const auto sourceIdx = d->filterModel->mapToSource(selection.at(0));
    d->model->removeIndex(sourceIdx);
}

void SSHManagerTreeWidget::editSshInfo()
{
    auto selection = ui->treeView->selectionModel()->selectedIndexes();
    if (selection.empty()) {
        return;
    }

    clearSshInfo();
    showInfoPane();

    const auto sourceIdx = d->filterModel->mapToSource(selection.at(0));
    const auto item = d->model->itemFromIndex(sourceIdx);
    const auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();

    ui->hostname->setText(data.host);
    ui->name->setText(data.name);
    ui->port->setText(data.port);
    ui->sshkey->setText(data.sshKey);
    ui->profile->setCurrentText(data.profileName);
    ui->username->setText(data.username);
    ui->useSshConfig->setCheckState(data.useSshConfig ? Qt::Checked : Qt::Unchecked);

    setEditComponentsEnabled(!data.importedFromSshConfig);

    // This is just for add. To edit the folder, the user will drag & drop.
    ui->folder->setCurrentText(QStringLiteral("not-used-here"));
    ui->folderLabel->hide();
    ui->folder->hide();
    ui->btnAdd->setText(tr("Update"));
    disconnect(ui->btnAdd, nullptr, this, nullptr);
    connect(ui->btnAdd, &QPushButton::clicked, this, &SSHManagerTreeWidget::saveEdit);

    handleImportedData(data.importedFromSshConfig);
}

void SSHManagerTreeWidget::handleImportedData(bool isImported)
{
    if (isImported) {
        ui->errorPanel->setText(QStringLiteral("You are currently viewing an imported SSH Profile, those are read only."));
        ui->errorPanel->show();
        ui->btnAdd->hide();
    } else {
        ui->errorPanel->hide();
    }
}

void SSHManagerTreeWidget::setEditComponentsEnabled(bool enabled)
{
    ui->hostname->setEnabled(enabled);
    ui->name->setEnabled(enabled);
    ui->port->setEnabled(enabled);
    ui->sshkey->setEnabled(enabled);
    ui->profile->setEnabled(enabled);
    ui->username->setEnabled(enabled);
    ui->useSshConfig->setEnabled(enabled);
}

void SSHManagerTreeWidget::clearSshInfo()
{
    hideInfoPane();
    ui->name->setText({});
    ui->hostname->setText({});
    ui->port->setText({});
    ui->sshkey->setText({});
}

void SSHManagerTreeWidget::hideInfoPane()
{
    ui->newSSHConfig->show();
    ui->btnRemove->show();
    ui->btnImport->show();
    ui->btnEdit->show();
    ui->sshInfoPane->hide();
    ui->btnAdd->hide();
    ui->btnCancel->hide();
    ui->errorPanel->hide();
}

void SSHManagerTreeWidget::showInfoPane()
{
    ui->newSSHConfig->hide();
    ui->btnRemove->hide();
    ui->btnImport->hide();
    ui->btnEdit->hide();
    ui->sshInfoPane->show();
    ui->btnAdd->show();
    ui->btnCancel->show();
    ui->folder->show();
    ui->folderLabel->show();

    ui->sshkey->setText({});

    ui->folder->clear();
    ui->folder->addItems(d->model->folders());

    setEditComponentsEnabled(true);
    ui->btnAdd->setText(tr("Add"));
    disconnect(ui->btnAdd, nullptr, this, nullptr);
    connect(ui->btnAdd, &QPushButton::clicked, this, &SSHManagerTreeWidget::addSshInfo);
}

void SSHManagerTreeWidget::setModel(SSHManagerModel *model)
{
    d->model = model;
    d->filterModel->setSourceModel(model);
    ui->folder->addItems(d->model->folders());
}

void SSHManagerTreeWidget::setCurrentController(Konsole::SessionController *controller)
{
    qCDebug(KonsoleDebug) << "Controller changed to" << controller;
    d->controller = controller;
}

std::pair<bool, QString> SSHManagerTreeWidget::checkFields() const
{
    bool error = false;
    QString errorString = QStringLiteral("<ul>");
    const QString li = QStringLiteral("<li>");
    const QString il = QStringLiteral("</li>");

    if (ui->hostname->text().isEmpty()) {
        error = true;
        errorString += li + i18n("Missing Hostname") + il;
    }

    if (ui->name->text().isEmpty()) {
        error = true;
        errorString += li + i18n("Missing Name") + il;
    }

    if (ui->port->text().isEmpty()) {
        error = true;
        errorString += li + i18n("Missing Port") + il;
    }

    if (ui->useSshConfig->checkState() == Qt::Checked) {
        if (ui->sshkey->text().count() || ui->username->text().count()) {
            error = true;
            errorString += li + i18n("If Use Ssh Config is set, do not specify sshkey or username.") + il;
        }
    } else {
        if (ui->sshkey->text().isEmpty() && ui->username->text().isEmpty()) {
            error = true;
            errorString += li + i18n("At least Username or SSHKey must be set") + il;
        }
    }

    if (ui->folder->currentText().isEmpty()) {
        error = true;
        errorString += li + i18n("Missing Folder") + il;
    }

    if (ui->profile->currentText().isEmpty()) {
        error = true;
        errorString += li + i18n("An SSH session must have a profile") + il;
    }
    errorString += QStringLiteral("</ul>");

    return {error, errorString};
}

void SSHManagerTreeWidget::handleTreeClick(Qt::MouseButton btn, const QModelIndex idx)
{
    if (!d->controller) {
        return;
    }
    auto sourceIdx = d->filterModel->mapToSource(idx);

    ui->treeView->setCurrentIndex(idx);
    ui->treeView->selectionModel()->setCurrentIndex(idx, QItemSelectionModel::SelectionFlag::Rows);

    if (btn == Qt::LeftButton || btn == Qt::RightButton) {
        const bool isParent = sourceIdx.parent() == d->model->invisibleRootItem()->index();

        if (isParent) {
            setEditComponentsEnabled(false);
            ui->btnEdit->setEnabled(false);
            if (ui->sshInfoPane->isVisible()) {
                ui->errorPanel->setText(i18n("Double click to change the folder name."));
            }
        } else {
            const auto item = d->model->itemFromIndex(sourceIdx);
            const auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
            ui->btnEdit->setEnabled(!data.importedFromSshConfig);

            if (ui->sshInfoPane->isVisible()) {
                handleImportedData(data.importedFromSshConfig);
                editSshInfo();
            }
        }
        return;
    }

    if (btn == Qt::MiddleButton) {
        if (sourceIdx.parent() == d->model->invisibleRootItem()->index()) {
            return;
        }

        Q_EMIT requestNewTab();
        connectRequested(idx);
    }
}

void SSHManagerTreeWidget::connectRequested(const QModelIndex &idx)
{
    if (!d->controller) {
        return;
    }

    auto sourceIdx = d->filterModel->mapToSource(idx);
    if (sourceIdx.parent() == d->model->invisibleRootItem()->index()) {
        return;
    }

    auto item = d->model->itemFromIndex(sourceIdx);
    auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();

    QString sshCommand = QStringLiteral("ssh ");
    if (!data.useSshConfig) {
        if (data.sshKey.length()) {
            sshCommand += QStringLiteral("-i %1 ").arg(data.sshKey);
        }

        if (data.port.length()) {
            sshCommand += QStringLiteral("-p %1 ").arg(data.port);
        }

        if (!data.username.isEmpty()) {
            sshCommand += data.username + QLatin1Char('@');
        }
    }

    if (!data.host.isEmpty()) {
        sshCommand += data.host;
    }

    d->controller->session()->sendTextToTerminal(sshCommand, QLatin1Char('\r'));
    if (d->controller->session()->views().count()) {
        d->controller->session()->views().at(0)->setFocus();
    }
}
