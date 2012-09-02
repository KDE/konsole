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
#include "ManageProfilesDialog.h"

// Qt
#include <QtCore/QFileInfo>
#include <QStandardItem>

// KDE
#include <KKeySequenceWidget>
#include <KStandardDirs>
#include <KDebug>

// Konsole
#include "EditProfileDialog.h"
#include "ProfileManager.h"
#include "ui_ManageProfilesDialog.h"

using namespace Konsole;

ManageProfilesDialog::ManageProfilesDialog(QWidget* aParent)
    : KDialog(aParent)
    , _sessionModel(new QStandardItemModel(this))
{
    setCaption(i18nc("@title:window", "Manage Profiles"));
    setButtons(KDialog::Close);

    connect(this, SIGNAL(finished()),
            ProfileManager::instance(), SLOT(saveSettings()));

    _ui = new Ui::ManageProfilesDialog();
    _ui->setupUi(mainWidget());

    // hide vertical header
    _ui->sessionTable->verticalHeader()->hide();
    _ui->sessionTable->setShowGrid(false);

    _ui->sessionTable->setItemDelegateForColumn(FavoriteStatusColumn, new FavoriteItemDelegate(this));
    _ui->sessionTable->setItemDelegateForColumn(ShortcutColumn, new ShortcutItemDelegate(this));
    _ui->sessionTable->setEditTriggers(_ui->sessionTable->editTriggers() | QAbstractItemView::SelectedClicked);

    // populate the table with profiles
    populateTable();

    // listen for changes to profiles
    connect(ProfileManager::instance(), SIGNAL(profileAdded(Profile::Ptr)), this,
            SLOT(addItems(Profile::Ptr)));
    connect(ProfileManager::instance(), SIGNAL(profileRemoved(Profile::Ptr)), this,
            SLOT(removeItems(Profile::Ptr)));
    connect(ProfileManager::instance(), SIGNAL(profileChanged(Profile::Ptr)), this,
            SLOT(updateItems(Profile::Ptr)));
    connect(ProfileManager::instance() ,
            SIGNAL(favoriteStatusChanged(Profile::Ptr,bool)), this,
            SLOT(updateFavoriteStatus(Profile::Ptr,bool)));

    // resize the session table to the full width of the table
    _ui->sessionTable->horizontalHeader()->setHighlightSections(false);
    _ui->sessionTable->resizeColumnsToContents();

    // allow a larger width for the shortcut column to account for the
    // increased with needed by the shortcut editor compared with just
    // displaying the text of the shortcut
    _ui->sessionTable->setColumnWidth(ShortcutColumn,
                                      _ui->sessionTable->columnWidth(ShortcutColumn) + 100);

    // setup buttons
    connect(_ui->newProfileButton, SIGNAL(clicked()), this, SLOT(createProfile()));
    connect(_ui->editProfileButton, SIGNAL(clicked()), this, SLOT(editSelected()));
    connect(_ui->deleteProfileButton, SIGNAL(clicked()), this, SLOT(deleteSelected()));
    connect(_ui->setAsDefaultButton, SIGNAL(clicked()), this, SLOT(setSelectedAsDefault()));
}

void ManageProfilesDialog::showEvent(QShowEvent*)
{
    Q_ASSERT(_ui->sessionTable->model());

    // try to ensure that all the text in all the columns is visible initially.
    // FIXME:  this is not a good solution, look for a more correct way to do this

    int totalWidth = 0;
    const int columnCount = _ui->sessionTable->model()->columnCount();

    for (int i = 0 ; i < columnCount ; i++)
        totalWidth += _ui->sessionTable->columnWidth(i);

    // the margin is added to account for the space taken by the resize grips
    // between the columns, this ensures that a horizontal scroll bar is not added
    // automatically
    int margin = style()->pixelMetric(QStyle::PM_HeaderGripMargin) * columnCount;
    _ui->sessionTable->setMinimumWidth(totalWidth + margin);
    _ui->sessionTable->horizontalHeader()->setStretchLastSection(true);
}

ManageProfilesDialog::~ManageProfilesDialog()
{
    delete _ui;
}

void ManageProfilesDialog::itemDataChanged(QStandardItem* item)
{
    if (item->column() == ShortcutColumn) {
        QKeySequence sequence = QKeySequence::fromString(item->text());
        ProfileManager::instance()->setShortcut(item->data(ShortcutRole).value<Profile::Ptr>(),
                                                sequence);
    } else if (item->column() == ProfileNameColumn) {
        QString newName = item->text();
        Profile::Ptr profile = item->data(ProfileKeyRole).value<Profile::Ptr>();
        QString oldName = profile->name();

        if (newName != oldName) {
            QHash<Profile::Property, QVariant> properties;
            properties.insert(Profile::Name, newName);
            properties.insert(Profile::UntranslatedName, newName);

            ProfileManager::instance()->changeProfile(profile, properties);
        }
    }
}

int ManageProfilesDialog::rowForProfile(const Profile::Ptr profile) const
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
void ManageProfilesDialog::removeItems(const Profile::Ptr profile)
{
    int row = rowForProfile(profile);
    if (row < 0)
        return;

    _sessionModel->removeRow(row);
}
void ManageProfilesDialog::updateItems(const Profile::Ptr profile)
{
    const int row = rowForProfile(profile);
    if (row < 0)
        return;

    QList<QStandardItem*> items;
    items << _sessionModel->item(row, ProfileNameColumn);
    items << _sessionModel->item(row, FavoriteStatusColumn);
    items << _sessionModel->item(row, ShortcutColumn);

    updateItemsForProfile(profile, items);
}
void ManageProfilesDialog::updateItemsForProfile(const Profile::Ptr profile, QList<QStandardItem*>& items) const
{
    // Profile Name
    items[ProfileNameColumn]->setText(profile->name());
    if (!profile->icon().isEmpty())
        items[ProfileNameColumn]->setIcon(KIcon(profile->icon()));
    items[ProfileNameColumn]->setData(QVariant::fromValue(profile), ProfileKeyRole);
    items[ProfileNameColumn]->setToolTip(i18n("Click to rename profile"));

    // Favorite Status
    const bool isFavorite = ProfileManager::instance()->findFavorites().contains(profile);
    if (isFavorite)
        items[FavoriteStatusColumn]->setData(KIcon("dialog-ok-apply"), Qt::DecorationRole);
    else
        items[FavoriteStatusColumn]->setData(KIcon(), Qt::DecorationRole);
    items[FavoriteStatusColumn]->setData(QVariant::fromValue(profile), ProfileKeyRole);
    items[FavoriteStatusColumn]->setToolTip(i18n("Click to toggle status"));

    // Shortcut
    QString shortcut = ProfileManager::instance()->shortcut(profile).toString();
    items[ShortcutColumn]->setText(shortcut);
    items[ShortcutColumn]->setData(QVariant::fromValue(profile), ShortcutRole);
    items[ShortcutColumn]->setToolTip(i18n("Double click to change shortcut"));
}
void ManageProfilesDialog::addItems(const Profile::Ptr profile)
{
    if (profile->isHidden())
        return;

    QList<QStandardItem*> items;
    for (int i = 0; i < 3; i++)
        items << new QStandardItem;

    updateItemsForProfile(profile, items);
    _sessionModel->appendRow(items);
}
void ManageProfilesDialog::populateTable()
{
    Q_ASSERT(!_ui->sessionTable->model());

    _ui->sessionTable->setModel(_sessionModel);

    _sessionModel->clear();
    // setup session table
    _sessionModel->setHorizontalHeaderLabels(QStringList() << i18nc("@title:column Profile label", "Name")
            << i18nc("@title:column Display profile in file menu", "Show in Menu")
            << i18nc("@title:column Profile shortcut text", "Shortcut"));

    QList<Profile::Ptr> profiles = ProfileManager::instance()->allProfiles();
    ProfileManager::instance()->sortProfiles(profiles);

    foreach(const Profile::Ptr& profile, profiles) {
        addItems(profile);
    }
    updateDefaultItem();

    connect(_sessionModel, SIGNAL(itemChanged(QStandardItem*)), this,
            SLOT(itemDataChanged(QStandardItem*)));

    // listen for changes in the table selection and update the state of the form's buttons
    // accordingly.
    //
    // it appears that the selection model is changed when the model itself is replaced,
    // so the signals need to be reconnected each time the model is updated.
    connect(_ui->sessionTable->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)), this,
            SLOT(tableSelectionChanged(QItemSelection)));

    _ui->sessionTable->selectRow(0);
}
void ManageProfilesDialog::updateDefaultItem()
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();

    const int rowCount = _sessionModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        QStandardItem* item = _sessionModel->item(i);
        QFont itemFont = item->font();

        bool isDefault = (defaultProfile == item->data().value<Profile::Ptr>());

        if (isDefault && !itemFont.bold()) {
            item->setIcon(KIcon(defaultProfile->icon(), 0, QStringList("emblem-favorite")));
            itemFont.setBold(true);
            item->setFont(itemFont);
        } else if (!isDefault && itemFont.bold()) {
            item->setIcon(KIcon(defaultProfile->icon()));
            itemFont.setBold(false);
            item->setFont(itemFont);
        }
    }
}
void ManageProfilesDialog::tableSelectionChanged(const QItemSelection&)
{
    const int selectedRows = _ui->sessionTable->selectionModel()->selectedRows().count();
    const ProfileManager* manager = ProfileManager::instance();
    const bool isNotDefault = (selectedRows > 0) && currentProfile() != manager->defaultProfile();
    const bool isDeletable = (selectedRows > 1) ||
                             (selectedRows == 1 && isProfileDeletable(currentProfile()));

    _ui->newProfileButton->setEnabled(selectedRows < 2);
    _ui->editProfileButton->setEnabled(selectedRows > 0);
    // do not allow the default session type to be removed
    _ui->deleteProfileButton->setEnabled(isDeletable && isNotDefault);
    _ui->setAsDefaultButton->setEnabled(isNotDefault && (selectedRows < 2));
}
void ManageProfilesDialog::deleteSelected()
{
    foreach(const Profile::Ptr & profile, selectedProfiles()) {
        if (profile != ProfileManager::instance()->defaultProfile())
            ProfileManager::instance()->deleteProfile(profile);
    }
}
void ManageProfilesDialog::setSelectedAsDefault()
{
    ProfileManager::instance()->setDefaultProfile(currentProfile());
    // do not allow the new default session type to be removed
    _ui->deleteProfileButton->setEnabled(false);
    _ui->setAsDefaultButton->setEnabled(false);

    // update font of new default item
    updateDefaultItem();
}

void ManageProfilesDialog::moveUpSelected()
{
    Q_ASSERT(_sessionModel);

    const int rowIndex = _ui->sessionTable->currentIndex().row();
    const QList<QStandardItem*>items = _sessionModel->takeRow(rowIndex);
    _sessionModel->insertRow(rowIndex - 1, items);
    _ui->sessionTable->selectRow(rowIndex - 1);
}

void ManageProfilesDialog::moveDownSelected()
{
    Q_ASSERT(_sessionModel);

    const int rowIndex = _ui->sessionTable->currentIndex().row();
    const QList<QStandardItem*>items = _sessionModel->takeRow(rowIndex);
    _sessionModel->insertRow(rowIndex + 1, items);
    _ui->sessionTable->selectRow(rowIndex + 1);
}

void ManageProfilesDialog::createProfile()
{
    // setup a temporary profile which is a clone of the selected profile
    // or the default if no profile is selected
    Profile::Ptr sourceProfile;

    Profile::Ptr selectedProfile = currentProfile();
    if (!selectedProfile)
        sourceProfile = ProfileManager::instance()->defaultProfile();
    else
        sourceProfile = selectedProfile;

    Q_ASSERT(sourceProfile);

    Profile::Ptr newProfile = Profile::Ptr(new Profile(ProfileManager::instance()->fallbackProfile()));
    newProfile->clone(sourceProfile, true);
    newProfile->setProperty(Profile::Name, i18nc("@item This will be used as part of the file name", "New Profile"));
    newProfile->setProperty(Profile::UntranslatedName, "New Profile");
    newProfile->setProperty(Profile::MenuIndex, QString("0"));

    QWeakPointer<EditProfileDialog> dialog = new EditProfileDialog(this);
    dialog.data()->setProfile(newProfile);
    dialog.data()->selectProfileName();

    if (dialog.data()->exec() == QDialog::Accepted) {
        ProfileManager::instance()->addProfile(newProfile);
        ProfileManager::instance()->setFavorite(newProfile, true);
        ProfileManager::instance()->changeProfile(newProfile, newProfile->setProperties());
    }
    delete dialog.data();
}
void ManageProfilesDialog::editSelected()
{
    EditProfileDialog dialog(this);
    // the dialog will delete the profile group when it is destroyed
    ProfileGroup* group = new ProfileGroup;
    foreach(const Profile::Ptr & profile, selectedProfiles()) {
        group->addProfile(profile);
    }
    group->updateValues();

    dialog.setProfile(Profile::Ptr(group));
    dialog.exec();
}
QList<Profile::Ptr> ManageProfilesDialog::selectedProfiles() const
{
    QList<Profile::Ptr> list;
    QItemSelectionModel* selection = _ui->sessionTable->selectionModel();
    if (!selection)
        return list;

    foreach(const QModelIndex & index, selection->selectedIndexes()) {
        if (index.column() == ProfileNameColumn)
            list << index.data(ProfileKeyRole).value<Profile::Ptr>();
    }

    return list;
}
Profile::Ptr ManageProfilesDialog::currentProfile() const
{
    QItemSelectionModel* selection = _ui->sessionTable->selectionModel();

    if (!selection || selection->selectedRows().count() != 1)
        return Profile::Ptr();

    return  selection->
            selectedIndexes().first().data(ProfileKeyRole).value<Profile::Ptr>();
}
bool ManageProfilesDialog::isProfileDeletable(Profile::Ptr profile) const
{
    static const QString systemDataLocation = KStandardDirs::installPath("data") + "konsole/";

    if (profile) {
        QFileInfo fileInfo(profile->path());

        if (fileInfo.exists()) {
            // never remove a system wide profile, no matter whether the
            // current user has enough permission
            if (profile->path().startsWith(systemDataLocation)) {
                return false;
            }

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
void ManageProfilesDialog::updateFavoriteStatus(Profile::Ptr profile, bool favorite)
{
    Q_ASSERT(_sessionModel);

    const int rowCount = _sessionModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        QModelIndex index = _sessionModel->index(i, FavoriteStatusColumn);
        if (index.data(ProfileKeyRole).value<Profile::Ptr>() == profile) {
            const KIcon icon = favorite ? KIcon("dialog-ok-apply") : KIcon();
            _sessionModel->setData(index, icon, Qt::DecorationRole);
        }
    }
}
void ManageProfilesDialog::setShortcutEditorVisible(bool visible)
{
    _ui->sessionTable->setColumnHidden(ShortcutColumn, !visible);
}
void StyledBackgroundPainter::drawBackground(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex&)
{
    const QStyleOptionViewItemV3* v3option = qstyleoption_cast<const QStyleOptionViewItemV3*>(&option);
    const QWidget* widget = v3option ? v3option->widget : 0;

    QStyle* style = widget ? widget->style() : QApplication::style();

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
        Profile::Ptr profile = index.data(ManageProfilesDialog::ProfileKeyRole).value<Profile::Ptr>();
        const bool isFavorite = ProfileManager::instance()->findFavorites().contains(profile);

        ProfileManager::instance()->setFavorite(profile, !isFavorite);
    }

    return true;
}
ShortcutItemDelegate::ShortcutItemDelegate(QObject* aParent)
    : QStyledItemDelegate(aParent)
{
}
void ShortcutItemDelegate::editorModified(const QKeySequence& keys)
{
    Q_UNUSED(keys);
    //kDebug() << keys.toString();

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

    if (!_modifiedEditors.contains(editor))
        return;

    QString shortcut = qobject_cast<KKeySequenceWidget*>(editor)->keySequence().toString();
    model->setData(index, shortcut, Qt::DisplayRole);

    _modifiedEditors.remove(editor);
}

QWidget* ShortcutItemDelegate::createEditor(QWidget* aParent, const QStyleOptionViewItem&, const QModelIndex& index) const
{
    _itemsBeingEdited.insert(index);

    KKeySequenceWidget* editor = new KKeySequenceWidget(aParent);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->setModifierlessAllowed(false);
    QString shortcutString = index.data(Qt::DisplayRole).toString();
    editor->setKeySequence(QKeySequence::fromString(shortcutString));
    connect(editor, SIGNAL(keySequenceChanged(QKeySequence)), this, SLOT(editorModified(QKeySequence)));
    editor->captureKeySequence();
    return editor;
}
void ShortcutItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                 const QModelIndex& index) const
{
    if (_itemsBeingEdited.contains(index))
        StyledBackgroundPainter::drawBackground(painter, option, index);
    else
        QStyledItemDelegate::paint(painter, option, index);
}

#include "ManageProfilesDialog.moc"
