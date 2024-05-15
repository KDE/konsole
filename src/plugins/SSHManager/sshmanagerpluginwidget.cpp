/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshmanagerpluginwidget.h"

#include "ProcessInfo.h"
#include "konsoledebug.h"
#include "session/Session.h"
#include "session/SessionController.h"

#include "sshmanagermodel.h"
#include "terminalDisplay/TerminalDisplay.h"

#include "profile/ProfileModel.h"

#include "sshmanagerfiltermodel.h"
#include "sshmanagerplugin.h"
#include "ui_sshwidget.h"

#include <KLocalizedString>
#include <KMessageBox>

#include <QAction>
#include <QFileDialog>
#include <QIntValidator>
#include <QItemSelectionModel>
#include <QMenu>
#include <QPoint>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

#include <QSettings>
#include <QSortFilterProxyModel>

#include <QStandardPaths>

struct SSHManagerTreeWidget::Private {
    SSHManagerModel *model = nullptr;
    SSHManagerFilterModel *filterModel = nullptr;
    Konsole::SessionController *controller = nullptr;
    bool isSetup = false;
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
    connect(ui->btnDelete, &QPushButton::clicked, this, &SSHManagerTreeWidget::triggerDelete);
    connect(ui->btnInvertFilter, &QPushButton::clicked, d->filterModel, &SSHManagerFilterModel::setInvertFilter);

    connect(ui->btnFindSshKey, &QPushButton::clicked, this, [this] {
        const QString homeFolder = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        const QString sshFile = QFileDialog::getOpenFileName(this, i18n("SSH Key"), homeFolder + QStringLiteral("/.ssh"));
        if (sshFile.isEmpty()) {
            return;
        }
        ui->sshkey->setText(sshFile);
    });

    connect(ui->filterText, &QLineEdit::textChanged, this, [this] {
        d->filterModel->setFilterRegularExpression(ui->filterText->text());
        d->filterModel->invalidate();
    });

    connect(Konsole::ProfileModel::instance(), &Konsole::ProfileModel::rowsRemoved, this, &SSHManagerTreeWidget::updateProfileList);
    connect(Konsole::ProfileModel::instance(), &Konsole::ProfileModel::rowsInserted, this, &SSHManagerTreeWidget::updateProfileList);
    updateProfileList();

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->treeView, &QTreeView::customContextMenuRequested, [this](const QPoint &pos) {
        QModelIndex idx = ui->treeView->indexAt(pos);
        if (!idx.isValid()) {
            return;
        }

        if (idx.data(Qt::DisplayRole) == i18n("SSH Config")) {
            return;
        }

        auto sourceIdx = d->filterModel->mapToSource(idx);
        const bool isParent = sourceIdx.parent() == d->model->invisibleRootItem()->index();
        if (!isParent) {
            const auto item = d->model->itemFromIndex(sourceIdx);
            const auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
            if (data.importedFromSshConfig) {
                return;
            }
        }

        QMenu *menu = new QMenu(this);
        auto action = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:inmenu", "Delete"), ui->treeView);
        menu->addAction(action);

        connect(action, &QAction::triggered, this, &SSHManagerTreeWidget::triggerDelete);

        menu->popup(ui->treeView->viewport()->mapToGlobal(pos));
    });

    connect(ui->treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &idx) {
        SSHManagerPlugin::requestConnection(d->filterModel, d->model, d->controller, idx);
    });

    connect(ui->treeView, &SshTreeView::mouseButtonClicked, this, &SSHManagerTreeWidget::handleTreeClick);

    ui->treeView->setModel(d->filterModel);

    // We have nothing selected, so there's nothing to edit.
    ui->btnEdit->setEnabled(false);

    clearSshInfo();

    QSettings settings;
    settings.beginGroup(QStringLiteral("plugins"));
    settings.beginGroup(QStringLiteral("sshplugin"));

    const QKeySequence def(Qt::CTRL | Qt::ALT | Qt::Key_H);
    const QString defText = def.toString();
    const QString entry = settings.value(QStringLiteral("ssh_shortcut"), defText).toString();
    const QKeySequence shortcutEntry(entry);

    connect(ui->keySequenceEdit, &QKeySequenceEdit::keySequenceChanged, this, [this] {
        auto shortcut = ui->keySequenceEdit->keySequence();
        Q_EMIT quickAccessShortcutChanged(shortcut);
    });
    ui->keySequenceEdit->setKeySequence(shortcutEntry);
}

SSHManagerTreeWidget::~SSHManagerTreeWidget() = default;

void SSHManagerTreeWidget::updateProfileList()
{
    ui->profile->clear();
    ui->profile->addItem(i18n("Don't Change"));
    auto model = Konsole::ProfileModel::instance();
    for (int i = 0, end = model->rowCount(QModelIndex()); i < end; i++) {
        const int column = Konsole::ProfileModel::Column::PROFILE;
        const int role = Qt::DisplayRole;
        const QModelIndex currIdx = model->index(i, column);
        const auto profileName = model->data(currIdx, role).toString();
        ui->profile->addItem(profileName);
    }
}

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
    //    SSHConfigurationData data; (not used?)
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
    // if ui->username is enabled then we were not imported!
    data.importedFromSshConfig = !ui->username->isEnabled();
    return data;
}

void SSHManagerTreeWidget::triggerDelete()
{
    auto selection = ui->treeView->selectionModel()->selectedIndexes();
    if (selection.empty()) {
        return;
    }

    const QString text = selection.at(0).data(Qt::DisplayRole).toString();
    const QString dialogMessage = ui->treeView->model()->rowCount(selection.at(0))
        ? i18n("You are about to delete the folder %1,\n with multiple SSH Configurations, are you sure?", text)
        : i18n("You are about to delete %1, are you sure?", text);

    const QString dontAskAgainKey =
        ui->treeView->model()->rowCount(selection.at(0)) ? QStringLiteral("remove_ssh_folder") : QStringLiteral("remove_ssh_config");

    int result = KMessageBox::warningTwoActions(this,
                                                dialogMessage,
                                                i18nc("@title:window", "Delete SSH Configurations"),
                                                KStandardGuiItem::del(),
                                                KStandardGuiItem::cancel(),
                                                dontAskAgainKey);

    if (result == KMessageBox::ButtonCode::SecondaryAction) {
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
    if (data.profileName.isEmpty()) {
        ui->profile->setCurrentIndex(0);
    } else {
        ui->profile->setCurrentText(data.profileName);
    }
    ui->username->setText(data.username);
    ui->useSshConfig->setCheckState(data.useSshConfig ? Qt::Checked : Qt::Unchecked);

    // This is just for add. To edit the folder, the user will drag & drop.
    ui->folder->setCurrentText(QStringLiteral("not-used-here"));
    ui->folderLabel->hide();
    ui->folder->hide();
    ui->btnAdd->setText(i18n("Update"));
    disconnect(ui->btnAdd, nullptr, this, nullptr);
    connect(ui->btnAdd, &QPushButton::clicked, this, &SSHManagerTreeWidget::saveEdit);

    handleImportedData(data.importedFromSshConfig);
}

void SSHManagerTreeWidget::handleImportedData(bool isImported)
{
    QList<QWidget *> elements = {ui->hostname, ui->port, ui->username, ui->sshkey, ui->useSshConfig};
    if (isImported) {
        ui->errorPanel->setText(i18n("Imported SSH Profile <br/> Some settings are read only."));
        ui->errorPanel->show();
    }

    for (auto *element : elements) {
        element->setEnabled(!isImported);
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
    ui->port->setText(QStringLiteral("22"));
    ui->sshkey->setText({});
    ui->treeView->setEnabled(true);
}

void SSHManagerTreeWidget::hideInfoPane()
{
    ui->newSSHConfig->show();
    ui->btnDelete->show();
    ui->btnEdit->show();
    ui->sshInfoPane->hide();
    ui->btnAdd->hide();
    ui->btnCancel->hide();
    ui->errorPanel->hide();
}

void SSHManagerTreeWidget::showInfoPane()
{
    ui->newSSHConfig->hide();
    ui->btnDelete->hide();
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
    ui->btnAdd->setText(i18n("Add"));
    disconnect(ui->btnAdd, nullptr, this, nullptr);
    connect(ui->btnAdd, &QPushButton::clicked, this, &SSHManagerTreeWidget::addSshInfo);

    // Disable the tree view when in edit mode.
    // This is important so the user don't click around
    // losing the configuration he did.
    // this will be enabled again when the user closes the panel.
    ui->treeView->setEnabled(false);
}

void SSHManagerTreeWidget::setModel(SSHManagerModel *model)
{
    d->model = model;
    d->filterModel->setSourceModel(model);
    ui->folder->addItems(d->model->folders());
    ui->btnManageProfile->setChecked(d->model->getManageProfile());
    connect(ui->btnManageProfile, &QPushButton::clicked, d->model, &SSHManagerModel::setManageProfile);
}

void SSHManagerTreeWidget::setCurrentController(Konsole::SessionController *controller)
{
    qCDebug(KonsoleDebug) << "Controller changed to" << controller;

    d->controller = controller;
    d->model->setSessionController(controller);
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

    if (ui->useSshConfig->checkState() == Qt::Checked) {
        // If ui->username is not enabled then this was an autopopulated entry and we should not complain
        if (ui->username->isEnabled() && (ui->sshkey->text().length() || ui->username->text().length())) {
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
            if (sourceIdx.data(Qt::DisplayRole).toString() == i18n("SSH Config")) {
                ui->btnDelete->setEnabled(false);
                ui->btnDelete->setToolTip(i18n("Cannot delete this folder"));
            } else {
                ui->btnDelete->setEnabled(true);
                ui->btnDelete->setToolTip(i18n("Delete folder and all of its contents"));
            }
            ui->btnEdit->setEnabled(false);
            if (ui->sshInfoPane->isVisible()) {
                ui->errorPanel->setText(i18n("Double click to change the folder name."));
            }
        } else {
            const auto item = d->model->itemFromIndex(sourceIdx);
            const auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
            ui->btnEdit->setEnabled(true);
            ui->btnDelete->setEnabled(!data.importedFromSshConfig);
            ui->btnDelete->setToolTip(data.importedFromSshConfig ? i18n("You can't delete an automatically added entry.") : i18n("Delete selected entry"));
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
        SSHManagerPlugin::requestConnection(d->filterModel, d->model, d->controller, sourceIdx);
    }
}

void SSHManagerTreeWidget::showEvent(QShowEvent *)
{
    if (!d->isSetup) {
        ui->treeView->expandAll();
        d->isSetup = true;
    }
}

#include "moc_sshmanagerpluginwidget.cpp"
