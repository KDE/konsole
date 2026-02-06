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
#include <QInputDialog>
#include <QIntValidator>
#include <QItemSelectionModel>
#include <QJsonDocument>
#include <QMenu>
#include <QPoint>
#include <QProcess>

#include <QSettings>
#include <QSortFilterProxyModel>

#include <QStandardPaths>

#include "sshmanagerplugindebug.h"

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
    ui->treeView->setSelectionMode(QAbstractItemView::ExtendedSelection);

    d->filterModel = new SSHManagerFilterModel(this);

    // full port range, users can setup ssh on any port they want
    const auto *portValidator = new QIntValidator(0, 65535, this);
    ui->port->setValidator(portValidator);

    connect(ui->newSSHConfig, &QPushButton::clicked, this, &SSHManagerTreeWidget::showInfoPane);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SSHManagerTreeWidget::clearSshInfo);
    connect(ui->btnEdit, &QPushButton::clicked, this, &SSHManagerTreeWidget::editSshInfo);
    connect(ui->btnDelete, &QPushButton::clicked, this, &SSHManagerTreeWidget::triggerDelete);
    connect(ui->btnInvertFilter, &QPushButton::clicked, d->filterModel, &SSHManagerFilterModel::setInvertFilter);
    connect(ui->btnShowPassword, &QToolButton::toggled, this, [this](bool checked) {
        ui->password->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        ui->btnShowPassword->setIcon(QIcon::fromTheme(checked ? QStringLiteral("view-hidden") : QStringLiteral("view-visible")));
    });
    connect(ui->btnShowSshKeyPassphrase, &QToolButton::toggled, this, [this](bool checked) {
        ui->sshKeyPassphrase->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        ui->btnShowSshKeyPassphrase->setIcon(QIcon::fromTheme(checked ? QStringLiteral("view-hidden") : QStringLiteral("view-visible")));
    });
    connect(ui->btnShowProxyPassword, &QToolButton::toggled, this, [this](bool checked) {
        ui->proxyPassword->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        ui->btnShowProxyPassword->setIcon(QIcon::fromTheme(checked ? QStringLiteral("view-hidden") : QStringLiteral("view-visible")));
    });

    connect(ui->btnEncrypt, &QCheckBox::toggled, this, &SSHManagerTreeWidget::toggleEncryption);
    connect(ui->btnChangeMasterPassword, &QPushButton::clicked, this, &SSHManagerTreeWidget::changeMasterPassword);
    connect(ui->btnExport, &QPushButton::clicked, this, &SSHManagerTreeWidget::exportProfiles);
    connect(ui->btnImport, &QPushButton::clicked, this, &SSHManagerTreeWidget::importProfiles);

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

        const auto item = d->model->itemFromIndex(sourceIdx);
        const auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
        const bool isImported = !isParent && data.importedFromSshConfig;

        QMenu *menu = new QMenu(this);

        if (!isParent) {
            auto duplicateAction = new QAction(QIcon::fromTheme(QStringLiteral("edit-copy")), i18nc("@action:inmenu", "Duplicate"), ui->treeView);
            menu->addAction(duplicateAction);
            connect(duplicateAction, &QAction::triggered, this, [this, sourceIdx] {
                const auto srcItem = d->model->itemFromIndex(sourceIdx);
                auto dataCopy = srcItem->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
                dataCopy.name += i18n(" (Copy)");
                dataCopy.importedFromSshConfig = false;
                const QString folderName = d->model->itemFromIndex(sourceIdx.parent())->text();
                d->model->addChildItem(dataCopy, folderName);
            });
        }

        if (!isImported) {
            auto action = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action:inmenu", "Delete"), ui->treeView);
            menu->addAction(action);
            connect(action, &QAction::triggered, this, &SSHManagerTreeWidget::triggerDelete);
        }

        if (!isParent) {
            const auto clearAction = new QAction(QIcon::fromTheme(QStringLiteral("document-edit")), i18nc("@action:inmenu", "Clear Host Key"), ui->treeView);
            menu->addAction(clearAction);
            connect(clearAction, &QAction::triggered, this, [this, sourceIdx] {
                const auto item = d->model->itemFromIndex(sourceIdx);
                const auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
                QString host = data.host;
                if (host.isEmpty()) return;
                
                // Handle [host]:port format if port is non-standard
                if (!data.port.isEmpty() && data.port != QStringLiteral("22")) {
                    host = QStringLiteral("[%1]:%2").arg(host, data.port);
                }

                QProcess process;
                process.start(QStringLiteral("ssh-keygen"), {QStringLiteral("-R"), host});
                process.waitForFinished();
                
                if (process.exitCode() == 0) {
                    KMessageBox::information(this, i18n("Host key for %1 removed successfully.", host), i18n("Host Key Removed"));
                } else {
                    KMessageBox::error(this, i18n("Failed to remove host key for %1.\nError: %2", host, QString::fromUtf8(process.readAllStandardError())), i18n("Error Removing Host Key"));
                }
            });
        }

        menu->popup(ui->treeView->viewport()->mapToGlobal(pos));
    });

    connect(ui->treeView, &QTreeView::doubleClicked, this, [this](const QModelIndex &idx) {
        if (!ensureDecrypted()) {
            return;
        }
        const auto selection = ui->treeView->selectionModel()->selectedIndexes();
        if (selection.size() > 1) {
            // Connect all selected profiles
            for (const auto &proxyIdx : selection) {
                auto sourceIdx = d->filterModel->mapToSource(proxyIdx);
                if (sourceIdx.parent() == d->model->invisibleRootItem()->index()) {
                    continue; // skip folders
                }
                Q_EMIT requestConnection(sourceIdx, d->controller);
            }
        } else {
            auto sourceIdx = d->filterModel->mapToSource(idx);
            Q_EMIT requestConnection(sourceIdx, d->controller);
        }
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
    data.sshKeyPassphrase = ui->sshKeyPassphrase->text().trimmed();
    data.profileName = ui->profile->currentText().trimmed();
    data.username = ui->username->text().trimmed();
    data.password = ui->password->text().trimmed();
    data.useSshConfig = ui->useSshConfig->checkState() == Qt::Checked;
    data.autoAcceptKeys = ui->autoAcceptKeys->checkState() == Qt::Checked;
    
    data.useProxy = ui->proxyGroup->isChecked();
    data.proxyIp = ui->proxyIp->text().trimmed();
    data.proxyPort = ui->proxyPort->text().trimmed();
    data.proxyUsername = ui->proxyUsername->text().trimmed();
    data.proxyPassword = ui->proxyPassword->text().trimmed();
    
    data.enableSshfs = ui->enableSshfs->isChecked();

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

    QString dialogMessage;
    QString dontAskAgainKey;

    if (selection.size() > 1) {
        dialogMessage = i18n("You are about to delete %1 entries, are you sure?", selection.size());
        dontAskAgainKey = QStringLiteral("remove_ssh_multiple");
    } else {
        const QString text = selection.at(0).data(Qt::DisplayRole).toString();
        dialogMessage = ui->treeView->model()->rowCount(selection.at(0))
            ? i18n("You are about to delete the folder %1,\n with multiple SSH Configurations, are you sure?", text)
            : i18n("You are about to delete %1, are you sure?", text);
        dontAskAgainKey =
            ui->treeView->model()->rowCount(selection.at(0)) ? QStringLiteral("remove_ssh_folder") : QStringLiteral("remove_ssh_config");
    }

    int result = KMessageBox::warningTwoActions(this,
                                                dialogMessage,
                                                i18nc("@title:window", "Delete SSH Configurations"),
                                                KStandardGuiItem::del(),
                                                KStandardGuiItem::cancel(),
                                                dontAskAgainKey);

    if (result == KMessageBox::ButtonCode::SecondaryAction) {
        return;
    }

    // Collect source indexes and sort in reverse row order so removal doesn't invalidate earlier indexes
    QList<QPersistentModelIndex> toRemove;
    for (const auto &proxyIdx : selection) {
        toRemove.append(QPersistentModelIndex(d->filterModel->mapToSource(proxyIdx)));
    }

    for (const auto &persistIdx : toRemove) {
        if (persistIdx.isValid()) {
            d->model->removeIndex(persistIdx);
        }
    }
}

void SSHManagerTreeWidget::editSshInfo()
{
    if (!ensureDecrypted()) {
        return;
    }
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
    ui->sshKeyPassphrase->setText(data.sshKeyPassphrase);
    if (data.profileName.isEmpty()) {
        ui->profile->setCurrentIndex(0);
    } else {
        ui->profile->setCurrentText(data.profileName);
    }
    ui->username->setText(data.username);
    ui->password->setText(data.password);
    ui->useSshConfig->setCheckState(data.useSshConfig ? Qt::Checked : Qt::Unchecked);
    ui->autoAcceptKeys->setCheckState(data.autoAcceptKeys ? Qt::Checked : Qt::Unchecked);
    
    ui->proxyGroup->setChecked(data.useProxy);
    ui->proxyIp->setText(data.proxyIp);
    ui->proxyPort->setText(data.proxyPort);
    ui->proxyUsername->setText(data.proxyUsername);
    ui->proxyPassword->setText(data.proxyPassword);

    ui->enableSshfs->setChecked(data.enableSshfs);

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
    QList<QWidget *> elements = {ui->hostname, ui->port, ui->username, ui->password, ui->sshkey, ui->sshKeyPassphrase, ui->useSshConfig, ui->autoAcceptKeys, ui->proxyGroup, ui->enableSshfs};
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
    ui->sshKeyPassphrase->setEnabled(enabled);
    ui->profile->setEnabled(enabled);
    ui->username->setEnabled(enabled);
    ui->password->setEnabled(enabled);
    ui->useSshConfig->setEnabled(enabled);
    ui->autoAcceptKeys->setEnabled(enabled);
    ui->proxyGroup->setEnabled(enabled);
    ui->enableSshfs->setEnabled(enabled);
}

void SSHManagerTreeWidget::clearSshInfo()
{
    hideInfoPane();
    ui->name->setText({});
    ui->hostname->setText({});
    ui->port->setText(QStringLiteral("22"));
    ui->sshkey->setText({});
    ui->sshKeyPassphrase->setText({});
    ui->password->setText({});
    ui->proxyGroup->setChecked(false);
    ui->proxyIp->setText({});
    ui->proxyPort->setText(QStringLiteral("1080"));
    ui->proxyUsername->setText({});
    ui->proxyPassword->setText({});
    ui->enableSshfs->setChecked(false);
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

    // Initialize encryption UI state
    const bool encrypted = d->model->isEncryptionEnabled();
    ui->btnEncrypt->blockSignals(true);
    ui->btnEncrypt->setChecked(encrypted);
    ui->btnEncrypt->blockSignals(false);
    ui->btnChangeMasterPassword->setEnabled(encrypted);
}

void SSHManagerTreeWidget::setCurrentController(Konsole::SessionController *controller)
{
    qCDebug(SshManagerPluginDebug) << "Controller changed to" << controller;

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

    // Don't override Qt's selection — ExtendedSelection handles Ctrl/Shift clicks

    if (btn == Qt::LeftButton || btn == Qt::RightButton) {
        const auto selection = ui->treeView->selectionModel()->selectedIndexes();
        const int selCount = selection.size();

        if (selCount > 1) {
            // Multiple items selected: disable edit, allow delete if none are imported
            ui->btnEdit->setEnabled(false);
            bool canDelete = true;
            for (const auto &proxyIdx : selection) {
                auto srcIdx = d->filterModel->mapToSource(proxyIdx);
                if (srcIdx.parent() == d->model->invisibleRootItem()->index()) {
                    continue; // folders are deletable (unless SSH Config)
                }
                const auto item = d->model->itemFromIndex(srcIdx);
                const auto data = item->data(SSHManagerModel::SSHRole).value<SSHConfigurationData>();
                if (data.importedFromSshConfig) {
                    canDelete = false;
                    break;
                }
            }
            ui->btnDelete->setEnabled(canDelete);
            ui->btnDelete->setToolTip(canDelete ? i18n("Delete selected entries") : i18n("Selection contains imported entries that cannot be deleted."));
            return;
        }

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
        if (!ensureDecrypted()) {
            return;
        }

        Q_EMIT requestNewTab();
        Q_EMIT requestConnection(sourceIdx, d->controller);
    }
}

void SSHManagerTreeWidget::showEvent(QShowEvent *)
{
    if (!d->isSetup) {
        ui->treeView->expandAll();
        d->isSetup = true;
    }
}

// --- Encryption ---

bool SSHManagerTreeWidget::ensureDecrypted()
{
    if (!d->model || !d->model->isEncryptionEnabled() || d->model->hasMasterPassword()) {
        return true;
    }

    for (int attempt = 0; attempt < 3; ++attempt) {
        bool ok = false;
        const QString password = QInputDialog::getText(this,
                                                       i18n("Master Password"),
                                                       i18n("Enter master password to unlock SSH profiles:"),
                                                       QLineEdit::Password,
                                                       {},
                                                       &ok);
        if (!ok) {
            return false;
        }

        if (d->model->verifyMasterPassword(password)) {
            d->model->setMasterPassword(password);
            d->model->decryptAll();
            return true;
        }

        KMessageBox::error(this, i18n("Incorrect master password."));
    }

    return false;
}

void SSHManagerTreeWidget::toggleEncryption(bool enabled)
{
    if (!d->model) {
        return;
    }

    if (enabled) {
        bool ok = false;
        const QString password = QInputDialog::getText(this,
                                                       i18n("Set Master Password"),
                                                       i18n("Enter a master password to encrypt stored passwords:"),
                                                       QLineEdit::Password,
                                                       {},
                                                       &ok);
        if (!ok || password.isEmpty()) {
            ui->btnEncrypt->blockSignals(true);
            ui->btnEncrypt->setChecked(false);
            ui->btnEncrypt->blockSignals(false);
            return;
        }

        // Confirm password
        const QString confirm = QInputDialog::getText(this,
                                                      i18n("Confirm Master Password"),
                                                      i18n("Confirm the master password:"),
                                                      QLineEdit::Password);
        if (confirm != password) {
            KMessageBox::error(this, i18n("Passwords do not match."));
            ui->btnEncrypt->blockSignals(true);
            ui->btnEncrypt->setChecked(false);
            ui->btnEncrypt->blockSignals(false);
            return;
        }

        d->model->enableEncryption(password);
        ui->btnChangeMasterPassword->setEnabled(true);
    } else {
        // Verify current password before disabling
        if (d->model->hasMasterPassword()) {
            d->model->disableEncryption();
        } else {
            bool ok = false;
            const QString password = QInputDialog::getText(this,
                                                           i18n("Master Password"),
                                                           i18n("Enter master password to disable encryption:"),
                                                           QLineEdit::Password,
                                                           {},
                                                           &ok);
            if (!ok || !d->model->verifyMasterPassword(password)) {
                KMessageBox::error(this, i18n("Incorrect password. Encryption remains enabled."));
                ui->btnEncrypt->blockSignals(true);
                ui->btnEncrypt->setChecked(true);
                ui->btnEncrypt->blockSignals(false);
                return;
            }
            d->model->setMasterPassword(password);
            d->model->disableEncryption();
        }
        ui->btnChangeMasterPassword->setEnabled(false);
    }
}

void SSHManagerTreeWidget::changeMasterPassword()
{
    if (!d->model) {
        return;
    }

    // Verify old password if not already unlocked
    if (!d->model->hasMasterPassword()) {
        bool ok = false;
        const QString oldPass = QInputDialog::getText(this,
                                                      i18n("Current Password"),
                                                      i18n("Enter current master password:"),
                                                      QLineEdit::Password,
                                                      {},
                                                      &ok);
        if (!ok || !d->model->verifyMasterPassword(oldPass)) {
            KMessageBox::error(this, i18n("Incorrect password."));
            return;
        }
        d->model->setMasterPassword(oldPass);
        d->model->decryptAll();
    }

    bool ok = false;
    const QString newPass = QInputDialog::getText(this,
                                                  i18n("New Master Password"),
                                                  i18n("Enter new master password:"),
                                                  QLineEdit::Password,
                                                  {},
                                                  &ok);
    if (!ok || newPass.isEmpty()) {
        return;
    }

    const QString confirm = QInputDialog::getText(this,
                                                  i18n("Confirm New Password"),
                                                  i18n("Confirm the new master password:"),
                                                  QLineEdit::Password);
    if (confirm != newPass) {
        KMessageBox::error(this, i18n("Passwords do not match."));
        return;
    }

    // Re-encrypt everything with new password
    d->model->enableEncryption(newPass);
    KMessageBox::information(this, i18n("Master password changed successfully."));
}

// --- Import/Export slots ---

void SSHManagerTreeWidget::exportProfiles()
{
    if (!d->model) {
        return;
    }

    if (!ensureDecrypted()) {
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(this,
                                                          i18n("Export SSH Profiles"),
                                                          QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QStringLiteral("/ssh-profiles.json"),
                                                          i18n("JSON Files (*.json)"));
    if (filePath.isEmpty()) {
        return;
    }

    // Ask if they want to encrypt the export
    QString exportPassword;
    int result = KMessageBox::questionTwoActions(this,
                                                 i18n("Do you want to encrypt the exported file with a passphrase?"),
                                                 i18n("Encrypt Export"),
                                                 KGuiItem(i18n("Encrypt")),
                                                 KGuiItem(i18n("No Encryption")));
    if (result == KMessageBox::PrimaryAction) {
        bool ok = false;
        exportPassword = QInputDialog::getText(this,
                                               i18n("Export Passphrase"),
                                               i18n("Enter a passphrase to encrypt the export:"),
                                               QLineEdit::Password,
                                               {},
                                               &ok);
        if (!ok || exportPassword.isEmpty()) {
            return;
        }

        const QString confirm = QInputDialog::getText(this,
                                                      i18n("Confirm Passphrase"),
                                                      i18n("Confirm the export passphrase:"),
                                                      QLineEdit::Password);
        if (confirm != exportPassword) {
            KMessageBox::error(this, i18n("Passphrases do not match."));
            return;
        }
    }

    const QJsonDocument doc = d->model->exportToJson(exportPassword);
    if (doc.isNull()) {
        KMessageBox::error(this, i18n("Failed to export profiles."));
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        KMessageBox::error(this, i18n("Could not open file for writing: %1", filePath));
        return;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();

    KMessageBox::information(this, i18n("Profiles exported successfully to %1.", filePath));
}

void SSHManagerTreeWidget::importProfiles()
{
    if (!d->model) {
        return;
    }

    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          i18n("Import SSH Profiles"),
                                                          QStandardPaths::writableLocation(QStandardPaths::HomeLocation),
                                                          i18n("JSON Files (*.json)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        KMessageBox::error(this, i18n("Could not open file: %1", filePath));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();

    if (parseError.error != QJsonParseError::NoError) {
        KMessageBox::error(this, i18n("Invalid JSON file: %1", parseError.errorString()));
        return;
    }

    QString importPassword;
    if (doc.isObject() && doc.object()[QStringLiteral("encrypted")].toBool()) {
        bool ok = false;
        importPassword = QInputDialog::getText(this,
                                               i18n("Import Passphrase"),
                                               i18n("This file is encrypted. Enter the passphrase:"),
                                               QLineEdit::Password,
                                               {},
                                               &ok);
        if (!ok) {
            return;
        }
    }

    if (!d->model->importFromJson(doc, importPassword)) {
        KMessageBox::error(this, i18n("Failed to import profiles. The file may be corrupted or the passphrase is incorrect."));
        return;
    }

    KMessageBox::information(this, i18n("Profiles imported successfully."));
}

#include "moc_sshmanagerpluginwidget.cpp"
