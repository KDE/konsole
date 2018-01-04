/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "ProfileSettings.h"

// Qt
#include <QFileInfo>
#include <QStandardPaths>
#include <QStandardItem>

// KDE
#include <KKeySequenceWidget>
#include <KLocalizedString>
#include <KIconLoader>
#include <QPushButton>

// Konsole
#include "EditProfileDialog.h"
#include "ProfileManager.h"
#include "Session.h"
#include "TerminalDisplay.h"
#include "SessionManager.h"
#include "SessionController.h"

using namespace Konsole;

ProfileSettings::ProfileSettings(QWidget* aParent)
    : QWidget(aParent)
    , _sessionModel(new QStandardItemModel(this))
{
    setupUi(this);

    // hide vertical header
    sessionTable->verticalHeader()->hide();
    sessionTable->setShowGrid(false);

    sessionTable->setItemDelegateForColumn(FavoriteStatusColumn, new FavoriteItemDelegate(this));
    sessionTable->setItemDelegateForColumn(ShortcutColumn, new ShortcutItemDelegate(this));
    sessionTable->setEditTriggers(sessionTable->editTriggers() | QAbstractItemView::SelectedClicked);

    // double clicking the profile name opens the profile edit dialog
    connect(sessionTable, &QTableView::doubleClicked, this, &Konsole::ProfileSettings::doubleClicked);

    // populate the table with profiles
    populateTable();

    // listen for changes to profiles
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileAdded, this, &Konsole::ProfileSettings::addItems);
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileRemoved, this, &Konsole::ProfileSettings::removeItems);
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileChanged, this, &Konsole::ProfileSettings::updateItems);
    connect(ProfileManager::instance() , &Konsole::ProfileManager::favoriteStatusChanged, this, &Konsole::ProfileSettings::updateFavoriteStatus);

    // resize the session table to the full width of the table
    sessionTable->horizontalHeader()->setHighlightSections(false);
    sessionTable->horizontalHeader()->setStretchLastSection(true);
    sessionTable->resizeColumnsToContents();

    // setup buttons
    connect(newProfileButton, &QPushButton::clicked, this, &Konsole::ProfileSettings::createProfile);
    connect(editProfileButton, &QPushButton::clicked, this, &Konsole::ProfileSettings::editSelected);
    connect(deleteProfileButton, &QPushButton::clicked, this, &Konsole::ProfileSettings::deleteSelected);
    connect(setAsDefaultButton, &QPushButton::clicked, this, &Konsole::ProfileSettings::setSelectedAsDefault);
}

ProfileSettings::~ProfileSettings() = default;

void ProfileSettings::slotAccepted()
{
    ProfileManager::instance()->saveSettings();
    deleteLater();
}

void ProfileSettings::itemDataChanged(QStandardItem* item)
{
    if (item->column() == ShortcutColumn) {
        QKeySequence sequence = QKeySequence::fromString(item->text());
        ProfileManager::instance()->setShortcut(item->data(ShortcutRole).value<Profile::Ptr>(),
                                                sequence);
    }
}

int ProfileSettings::rowForProfile(const Profile::Ptr profile) const
{
    const int rowCount = _sessionModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        if (_sessionModel->item(i, ProfileNameColumn)->data(ProfileKeyRole)
                .value<Profile::Ptr>() == profile) {
            return i;
        }
    }
    return -1;
}
void ProfileSettings::removeItems(const Profile::Ptr profile)
{
    int row = rowForProfile(profile);
    if (row < 0) {
        return;
    }

    _sessionModel->removeRow(row);
}
void ProfileSettings::updateItems(const Profile::Ptr profile)
{
    const int row = rowForProfile(profile);
    if (row < 0) {
        return;
    }

    QList<QStandardItem*> items;
    items << _sessionModel->item(row, ProfileNameColumn);
    items << _sessionModel->item(row, FavoriteStatusColumn);
    items << _sessionModel->item(row, ShortcutColumn);

    updateItemsForProfile(profile, items);
}
void ProfileSettings::updateItemsForProfile(const Profile::Ptr profile, QList<QStandardItem*>& items) const
{
    // Profile Name
    items[ProfileNameColumn]->setText(profile->name());
    if (!profile->icon().isEmpty()) {
        items[ProfileNameColumn]->setIcon(QIcon::fromTheme(profile->icon()));
    }
    items[ProfileNameColumn]->setData(QVariant::fromValue(profile), ProfileKeyRole);
    // only allow renaming the profile from the edit profile dialog
    // so as to use ProfileManager::checkProfileName()
    items[ProfileNameColumn]->setEditable(false);

    // Favorite Status
    const bool isFavorite = ProfileManager::instance()->findFavorites().contains(profile);
    if (isFavorite) {
        items[FavoriteStatusColumn]->setData(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")), Qt::DecorationRole);
    } else {
        items[FavoriteStatusColumn]->setData(QIcon(), Qt::DecorationRole);
    }
    items[FavoriteStatusColumn]->setData(QVariant::fromValue(profile), ProfileKeyRole);
    items[FavoriteStatusColumn]->setToolTip(i18nc("@info:tooltip", "Click to toggle status"));

    // Shortcut
    QString shortcut = ProfileManager::instance()->shortcut(profile).toString();
    items[ShortcutColumn]->setText(shortcut);
    items[ShortcutColumn]->setData(QVariant::fromValue(profile), ShortcutRole);
    items[ShortcutColumn]->setToolTip(i18nc("@info:tooltip", "Double click to change shortcut"));
}

void ProfileSettings::doubleClicked(const QModelIndex &index)
{
    QStandardItem *item = _sessionModel->itemFromIndex(index);
    if (item->column() == ProfileNameColumn) {
        editSelected();
    }
}

void ProfileSettings::addItems(const Profile::Ptr profile)
{
    if (profile->isHidden()) {
        return;
    }

    QList<QStandardItem*> items;
    items.reserve(3);
    for (int i = 0; i < 3; i++) {
        items.append(new QStandardItem);
    }

    updateItemsForProfile(profile, items);
    _sessionModel->appendRow(items);
}
void ProfileSettings::populateTable()
{
    Q_ASSERT(!sessionTable->model());

    sessionTable->setModel(_sessionModel);

    _sessionModel->clear();
    // setup session table
    _sessionModel->setHorizontalHeaderLabels(QStringList() << i18nc("@title:column Profile label", "Name")
            << i18nc("@title:column Display profile in file menu", "Show")
            << i18nc("@title:column Profile shortcut text", "Shortcut"));

    QList<Profile::Ptr> profiles = ProfileManager::instance()->allProfiles();
    ProfileManager::instance()->sortProfiles(profiles);

    foreach(const Profile::Ptr& profile, profiles) {
        addItems(profile);
    }
    updateDefaultItem();

    connect(_sessionModel, &QStandardItemModel::itemChanged, this, &Konsole::ProfileSettings::itemDataChanged);

    // listen for changes in the table selection and update the state of the form's buttons
    // accordingly.
    //
    // it appears that the selection model is changed when the model itself is replaced,
    // so the signals need to be reconnected each time the model is updated.
    connect(sessionTable->selectionModel(), &QItemSelectionModel::selectionChanged, this, &Konsole::ProfileSettings::tableSelectionChanged);

    sessionTable->selectRow(0);
}
void ProfileSettings::updateDefaultItem()
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();

    const int rowCount = _sessionModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        QStandardItem* item = _sessionModel->item(i);
        QFont itemFont = item->font();

        bool isDefault = (defaultProfile == item->data().value<Profile::Ptr>());

        if (isDefault && !itemFont.bold()) {
            QIcon icon(KIconLoader::global()->loadIcon(defaultProfile->icon(), KIconLoader::Small, 0, KIconLoader::DefaultState, QStringList(QStringLiteral("emblem-favorite"))));
            item->setIcon(icon);
            itemFont.setBold(true);
            item->setFont(itemFont);
        } else if (!isDefault && itemFont.bold()) {
            item->setIcon(QIcon(defaultProfile->icon()));
            itemFont.setBold(false);
            item->setFont(itemFont);
        }
    }
}
void ProfileSettings::tableSelectionChanged(const QItemSelection&)
{
    const int selectedRows = sessionTable->selectionModel()->selectedRows().count();
    const ProfileManager* manager = ProfileManager::instance();
    const bool isNotDefault = (selectedRows > 0) && currentProfile() != manager->defaultProfile();
    const bool isDeletable = (selectedRows > 1) ||
                             (selectedRows == 1 && isProfileDeletable(currentProfile()));

    newProfileButton->setEnabled(selectedRows < 2);
    // FIXME: At some point editing 2+ profiles no longer works
    editProfileButton->setEnabled(selectedRows == 1);
    // do not allow the default session type to be removed
    deleteProfileButton->setEnabled(isDeletable && isNotDefault);
    setAsDefaultButton->setEnabled(isNotDefault && (selectedRows < 2));
}
void ProfileSettings::deleteSelected()
{
    foreach(const Profile::Ptr & profile, selectedProfiles()) {
        if (profile != ProfileManager::instance()->defaultProfile()) {
            ProfileManager::instance()->deleteProfile(profile);
        }
    }
}
void ProfileSettings::setSelectedAsDefault()
{
    ProfileManager::instance()->setDefaultProfile(currentProfile());
    // do not allow the new default session type to be removed
    deleteProfileButton->setEnabled(false);
    setAsDefaultButton->setEnabled(false);

    // update font of new default item
    updateDefaultItem();
}

void ProfileSettings::moveUpSelected()
{
    Q_ASSERT(_sessionModel);

    const int rowIndex = sessionTable->currentIndex().row();
    const QList<QStandardItem*>items = _sessionModel->takeRow(rowIndex);
    _sessionModel->insertRow(rowIndex - 1, items);
    sessionTable->selectRow(rowIndex - 1);
}

void ProfileSettings::moveDownSelected()
{
    Q_ASSERT(_sessionModel);

    const int rowIndex = sessionTable->currentIndex().row();
    const QList<QStandardItem*>items = _sessionModel->takeRow(rowIndex);
    _sessionModel->insertRow(rowIndex + 1, items);
    sessionTable->selectRow(rowIndex + 1);
}

void ProfileSettings::createProfile()
{
    // setup a temporary profile which is a clone of the selected profile
    // or the default if no profile is selected
    Profile::Ptr sourceProfile;

    Profile::Ptr selectedProfile = currentProfile();
    if (!selectedProfile) {
        sourceProfile = ProfileManager::instance()->defaultProfile();
    } else {
        sourceProfile = selectedProfile;
    }

    Q_ASSERT(sourceProfile);

    Profile::Ptr newProfile = Profile::Ptr(new Profile(ProfileManager::instance()->fallbackProfile()));
    newProfile->clone(sourceProfile, true);
    newProfile->setProperty(Profile::Name, i18nc("@item This will be used as part of the file name", "New Profile"));
    newProfile->setProperty(Profile::UntranslatedName, QStringLiteral("New Profile"));
    newProfile->setProperty(Profile::MenuIndex, QStringLiteral("0"));

    QPointer<EditProfileDialog> dialog = new EditProfileDialog(this);
    dialog.data()->setProfile(newProfile);
    dialog.data()->selectProfileName();

    if (dialog.data()->exec() == QDialog::Accepted) {
        ProfileManager::instance()->addProfile(newProfile);
        ProfileManager::instance()->setFavorite(newProfile, true);
        ProfileManager::instance()->changeProfile(newProfile, newProfile->setProperties());
    }
    delete dialog.data();
}
void ProfileSettings::editSelected()
{
    QList<Profile::Ptr> profiles(selectedProfiles());

    foreach (Session* session, SessionManager::instance()->sessions()) {
         foreach (TerminalDisplay* terminal, session->views()) {
             // Searching for opened profiles
             if (terminal->sessionController()->profileDialogPointer() != nullptr) {
                 foreach (const Profile::Ptr & profile, profiles) {
                     if (profile->name() == terminal->sessionController()->profileDialogPointer()->lookupProfile()->name()
                         && terminal->sessionController()->profileDialogPointer()->isVisible()) {
                         // close opened edit dialog
                         terminal->sessionController()->profileDialogPointer()->close();
                     }
                 }
             }
         }
    }

    EditProfileDialog dialog(this);
    // the dialog will delete the profile group when it is destroyed
    ProfileGroup* group = new ProfileGroup;
    foreach (const Profile::Ptr & profile, profiles) {
        group->addProfile(profile);
    }
    group->updateValues();

    dialog.setProfile(Profile::Ptr(group));
    dialog.exec();
}
QList<Profile::Ptr> ProfileSettings::selectedProfiles() const
{
    QList<Profile::Ptr> list;
    QItemSelectionModel* selection = sessionTable->selectionModel();
    if (selection == nullptr) {
        return list;
    }

    foreach(const QModelIndex & index, selection->selectedIndexes()) {
        if (index.column() == ProfileNameColumn) {
            list << index.data(ProfileKeyRole).value<Profile::Ptr>();
        }
    }

    return list;
}
Profile::Ptr ProfileSettings::currentProfile() const
{
    QItemSelectionModel* selection = sessionTable->selectionModel();

    if ((selection == nullptr) || selection->selectedRows().count() != 1) {
        return Profile::Ptr();
    }

    return  selection->
            selectedIndexes().first().data(ProfileKeyRole).value<Profile::Ptr>();
}
bool ProfileSettings::isProfileDeletable(Profile::Ptr profile) const
{
    if (profile) {
        QFileInfo fileInfo(profile->path());

        if (fileInfo.exists()) {
            // check whether user has enough permission
            QFileInfo dirInfo(fileInfo.path());
            return dirInfo.isWritable();
        } else {
            return true;
        }
    } else {
        return true;
    }
}
void ProfileSettings::updateFavoriteStatus(Profile::Ptr profile, bool favorite)
{
    Q_ASSERT(_sessionModel);

    const int rowCount = _sessionModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        QModelIndex index = _sessionModel->index(i, FavoriteStatusColumn);
        if (index.data(ProfileKeyRole).value<Profile::Ptr>() == profile) {
            const QIcon icon = favorite ? QIcon::fromTheme(QStringLiteral("dialog-ok-apply")) : QIcon();
            _sessionModel->setData(index, icon, Qt::DecorationRole);
        }
    }
}
void ProfileSettings::setShortcutEditorVisible(bool visible)
{
    sessionTable->setColumnHidden(ShortcutColumn, !visible);
}
void StyledBackgroundPainter::drawBackground(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex&)
{
    const QStyleOptionViewItemV3* v3option = qstyleoption_cast<const QStyleOptionViewItemV3*>(&option);
    const QWidget* widget = v3option != nullptr ? v3option->widget : nullptr;

    QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, widget);
}

FavoriteItemDelegate::FavoriteItemDelegate(QObject* aParent)
    : QStyledItemDelegate(aParent)
{
}
void FavoriteItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // See implementation of QStyledItemDelegate::paint()
    QStyleOptionViewItemV4 opt = option;
    initStyleOption(&opt, index);

    StyledBackgroundPainter::drawBackground(painter, opt, index);

    int margin = (opt.rect.height() - opt.decorationSize.height()) / 2;
    margin++;

    opt.rect.setTop(opt.rect.top() + margin);
    opt.rect.setBottom(opt.rect.bottom() - margin);

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(painter, opt.rect, Qt::AlignCenter);
}

bool FavoriteItemDelegate::editorEvent(QEvent* aEvent, QAbstractItemModel*,
                                       const QStyleOptionViewItem&, const QModelIndex& index)
{
    if (aEvent->type() == QEvent::MouseButtonPress ||
            aEvent->type() == QEvent::KeyPress ||
            aEvent->type() == QEvent::MouseButtonDblClick) {
        Profile::Ptr profile = index.data(ProfileSettings::ProfileKeyRole).value<Profile::Ptr>();
        const bool isFavorite = ProfileManager::instance()->findFavorites().contains(profile);

        ProfileManager::instance()->setFavorite(profile, !isFavorite);
    }

    return true;
}
ShortcutItemDelegate::ShortcutItemDelegate(QObject* aParent)
    : QStyledItemDelegate(aParent),
    _modifiedEditors(QSet<QWidget *>()),
    _itemsBeingEdited(QSet<QModelIndex>())
{
}
void ShortcutItemDelegate::editorModified(const QKeySequence& keys)
{
    Q_UNUSED(keys);

    KKeySequenceWidget* editor = qobject_cast<KKeySequenceWidget*>(sender());
    Q_ASSERT(editor);
    _modifiedEditors.insert(editor);
    emit commitData(editor);
    emit closeEditor(editor);
}
void ShortcutItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                        const QModelIndex& index) const
{
    _itemsBeingEdited.remove(index);

    if (!_modifiedEditors.contains(editor)) {
        return;
    }

    QString shortcut = qobject_cast<KKeySequenceWidget*>(editor)->keySequence().toString();
    model->setData(index, shortcut, Qt::DisplayRole);

    _modifiedEditors.remove(editor);
}

QWidget* ShortcutItemDelegate::createEditor(QWidget* aParent, const QStyleOptionViewItem&, const QModelIndex& index) const
{
    _itemsBeingEdited.insert(index);

    auto editor = new KKeySequenceWidget(aParent);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->setModifierlessAllowed(false);
    QString shortcutString = index.data(Qt::DisplayRole).toString();
    editor->setKeySequence(QKeySequence::fromString(shortcutString));
    connect(editor, &KKeySequenceWidget::keySequenceChanged, this, &Konsole::ShortcutItemDelegate::editorModified);
    editor->captureKeySequence();
    return editor;
}
void ShortcutItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    if (_itemsBeingEdited.contains(index)) {
        StyledBackgroundPainter::drawBackground(painter, option, index);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

