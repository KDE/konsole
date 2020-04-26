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
#include <QKeyEvent>

// Konsole
#include "EditProfileDialog.h"
#include "ProfileManager.h"
#include "Session.h"
#include "TerminalDisplay.h"
#include "SessionManager.h"
#include "SessionController.h"

using namespace Konsole;

ProfileSettings::ProfileSettings(QWidget* parent)
    : QWidget(parent)
    , _sessionModel(new QStandardItemModel(this))
{
    setupUi(this);

    profilesList->setItemDelegateForColumn(ShortcutColumn, new ShortcutItemDelegate(this));

    // double clicking the profile name opens the profile edit dialog
    connect(profilesList, &QAbstractItemView::doubleClicked, this, &Konsole::ProfileSettings::doubleClicked);

    // populate the table with profiles
    populateTable();

    // listen for changes to profiles
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileAdded, this, &Konsole::ProfileSettings::addItems);
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileRemoved, this, &Konsole::ProfileSettings::removeItems);
    connect(ProfileManager::instance(), &Konsole::ProfileManager::profileChanged, this, &Konsole::ProfileSettings::updateItems);
    connect(ProfileManager::instance(), &Konsole::ProfileManager::favoriteStatusChanged, this, &Konsole::ProfileSettings::updateFavoriteStatus);

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
        QStandardItem *idItem = item->model()->item(item->row(), ProfileColumn);
        ProfileManager::instance()->setShortcut(idItem->data(ProfilePtrRole).value<Profile::Ptr>(),
                                                sequence);
    } else if (item->column() == FavoriteStatusColumn) {
        QStandardItem *idItem = item->model()->item(item->row(), ProfileColumn);
        const bool isFavorite = item->checkState() == Qt::Checked;
        ProfileManager::instance()->setFavorite(idItem->data(ProfilePtrRole).value<Profile::Ptr>(),
                                                isFavorite);
        updateShortcutField(item->model()->item(item->row(), ShortcutColumn), isFavorite);
    }
}

void ProfileSettings::updateShortcutField(QStandardItem *item, bool isFavorite) const
{
    if(isFavorite) {
        item->setToolTip(i18nc("@info:tooltip", "Double click to change shortcut"));
        item->setForeground(palette().color(QPalette::Normal, QPalette::Text));
    } else {
        item->setToolTip(i18nc("@info:tooltip", "Shortcut won't work while the profile is not marked as visible."));
        item->setForeground(palette().color(QPalette::Disabled, QPalette::Text));
    }
}

int ProfileSettings::rowForProfile(const Profile::Ptr &profile) const
{
    const int rowCount = _sessionModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        if (_sessionModel->item(i, ProfileColumn)->data(ProfilePtrRole)
                .value<Profile::Ptr>() == profile) {
            return i;
        }
    }
    return -1;
}
void ProfileSettings::removeItems(const Profile::Ptr &profile)
{
    int row = rowForProfile(profile);
    if (row < 0) {
        return;
    }

    _sessionModel->removeRow(row);
}
void ProfileSettings::updateItems(const Profile::Ptr &profile)
{
    const int row = rowForProfile(profile);
    if (row < 0) {
        return;
    }

    const auto items = QList<QStandardItem*> {
        _sessionModel->item(row, FavoriteStatusColumn),
        _sessionModel->item(row, ProfileNameColumn),
        _sessionModel->item(row, ShortcutColumn),
        _sessionModel->item(row, ProfileColumn),
    };
    updateItemsForProfile(profile, items);
}
void ProfileSettings::updateItemsForProfile(const Profile::Ptr &profile, const QList<QStandardItem*>& items) const
{
    // "Enabled" checkbox
    const auto isEnabled = ProfileManager::instance()->findFavorites().contains(profile);
    items[FavoriteStatusColumn]->setCheckState(isEnabled ? Qt::Checked : Qt::Unchecked);
    items[FavoriteStatusColumn]->setCheckable(true);
    items[FavoriteStatusColumn]->setToolTip(
            i18nc("@info:tooltip List item's checkbox for making item (profile) visible in a menu",
                  "Show profile in menu"));

    // Profile Name
    items[ProfileNameColumn]->setText(profile->name());
    if (!profile->icon().isEmpty()) {
        items[ProfileNameColumn]->setIcon(QIcon::fromTheme(profile->icon()));
    }
    // only allow renaming the profile from the edit profile dialog
    // so as to use ProfileManager::checkProfileName()
    items[ProfileNameColumn]->setEditable(false);

    // Shortcut
    const auto shortcut = ProfileManager::instance()->shortcut(profile).toString();
    items[ShortcutColumn]->setText(shortcut);
    updateShortcutField(items[ShortcutColumn], isEnabled);

    // Profile ID (pointer to profile) - intended to be hidden in a view
    items[ProfileColumn]->setData(QVariant::fromValue(profile), ProfilePtrRole);
}

void ProfileSettings::doubleClicked(const QModelIndex &index)
{
    QStandardItem *item = _sessionModel->itemFromIndex(index);
    if (item->column() == ProfileNameColumn) {
        editSelected();
    }
}

void ProfileSettings::addItems(const Profile::Ptr &profile)
{
    if (profile->isHidden()) {
        return;
    }

    // each _sessionModel row has three items.
    const auto items = QList<QStandardItem*> {
        new QStandardItem(),
        new QStandardItem(),
        new QStandardItem(),
        new QStandardItem(),
    };

    updateItemsForProfile(profile, items);
    _sessionModel->appendRow(items);
}
void ProfileSettings::populateTable()
{
    Q_ASSERT(!profilesList->model());

    profilesList->setModel(_sessionModel);

    _sessionModel->clear();
    // setup session table
    _sessionModel->setHorizontalHeaderLabels({
        QString(), // set using header item below
        i18nc("@title:column Profile name", "Name"),
        i18nc("@title:column Profile keyboard shortcut", "Shortcut"),
        QString(),
    });
    auto *favoriteColumnHeaderItem = new QStandardItem();
    favoriteColumnHeaderItem->setIcon(QIcon::fromTheme(QStringLiteral("visibility")));
    favoriteColumnHeaderItem->setToolTip(
            i18nc("@info:tooltip List item's checkbox for making item (profile) visible in a menu",
                  "Show profile in menu"));
    _sessionModel->setHorizontalHeaderItem(FavoriteStatusColumn, favoriteColumnHeaderItem);

    // Calculate favorite column width. resizeColumnToContents()
    // is not used because it takes distance between checkbox and
    // text into account, but there is no text and it looks weird.
    const int headerMargin = style()->pixelMetric(QStyle::PM_HeaderMargin, nullptr,
                                            profilesList->header());
    const int iconWidth = style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr,
                                               profilesList->header());
    const int favoriteHeaderWidth = headerMargin * 2 + iconWidth;
    QStyleOptionViewItem opt;
    opt.features = QStyleOptionViewItem::HasCheckIndicator | QStyleOptionViewItem::HasDecoration;
    const QRect checkBoxRect = style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator,
                                                       &opt, profilesList);
    // When right edge is at x < 0 it is assumed the checkbox is
    // placed on the right item's side and the margin between right
    // checkbox edge and right item edge should be used.
    const int checkBoxMargin = checkBoxRect.right() >= 0 ? checkBoxRect.x()
                                                         : 0 - checkBoxRect.right();
    const int favoriteItemWidth = checkBoxMargin * 2 + checkBoxRect.width();
    auto *listHeader = profilesList->header();

    profilesList->setColumnWidth(FavoriteStatusColumn,
                                 qMax(favoriteHeaderWidth, favoriteItemWidth));
    profilesList->resizeColumnToContents(ProfileNameColumn);
    listHeader->setSectionResizeMode(FavoriteStatusColumn, QHeaderView::ResizeMode::Fixed);
    listHeader->setSectionResizeMode(ProfileNameColumn, QHeaderView::ResizeMode::Stretch);
    listHeader->setSectionResizeMode(ShortcutColumn, QHeaderView::ResizeMode::ResizeToContents);
    listHeader->setStretchLastSection(false);
    listHeader->setSectionsMovable(false);

    profilesList->hideColumn(ProfileColumn);

    QList<Profile::Ptr> profiles = ProfileManager::instance()->allProfiles();
    ProfileManager::instance()->sortProfiles(profiles);

    for (const Profile::Ptr &profile : qAsConst(profiles)) {
        addItems(profile);
    }
    updateDefaultItem();

    connect(_sessionModel, &QStandardItemModel::itemChanged, this, &Konsole::ProfileSettings::itemDataChanged);

    // listen for changes in the table selection and update the state of the form's buttons
    // accordingly.
    //
    // it appears that the selection model is changed when the model itself is replaced,
    // so the signals need to be reconnected each time the model is updated.
    connect(profilesList->selectionModel(), &QItemSelectionModel::selectionChanged, this, &Konsole::ProfileSettings::tableSelectionChanged);
}
void ProfileSettings::updateDefaultItem()
{
    Profile::Ptr defaultProfile = ProfileManager::instance()->defaultProfile();

    const QString defaultItemSuffix = i18nc("@item:intable Default list item's name suffix (with separator)", " (default)");

    const int rowCount = _sessionModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        QStandardItem* item = _sessionModel->item(i, ProfileNameColumn);
        QFont itemFont = item->font();
        QStandardItem* profileIdItem = _sessionModel->item(i, ProfileColumn);
        auto profile = profileIdItem->data().value<Profile::Ptr>();
        const bool isDefault = (defaultProfile == profile);
        const QString cleanItemName = profile != nullptr ? profile->name() : QString();

        if (isDefault) {
            itemFont.setItalic(true);
            item->setFont(itemFont);
            item->setText(cleanItemName + defaultItemSuffix);
        } else {
            // FIXME: use default font
            itemFont.setItalic(false);
            item->setFont(itemFont);
            item->setText(cleanItemName);
        }
    }
}
void ProfileSettings::tableSelectionChanged(const QItemSelection&)
{
    const ProfileManager* manager = ProfileManager::instance();
    bool isNotDefault = true;
    bool isDeletable = true;

    const auto profiles = selectedProfiles();
    for (const auto &profile: profiles) {
        isNotDefault = isNotDefault && (profile != manager->defaultProfile());
        isDeletable = isDeletable && isProfileDeletable(profile);
    }

    newProfileButton->setEnabled(profiles.count() < 2);
    // FIXME: At some point editing 2+ profiles no longer works
    editProfileButton->setEnabled(profiles.count() == 1);
    // do not allow the default session type to be removed
    deleteProfileButton->setEnabled(isDeletable && isNotDefault && (profiles.count() > 0));
    setAsDefaultButton->setEnabled(isNotDefault && (profiles.count() == 1));
}
void ProfileSettings::deleteSelected()
{
    const QList<Profile::Ptr> profiles = selectedProfiles();
    for (const Profile::Ptr &profile : profiles) {
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

void ProfileSettings::createProfile()
{
    // setup a temporary profile which is a clone of the selected profile
    // or the default if no profile is selected
    Profile::Ptr sourceProfile = currentProfile() ? currentProfile() : ProfileManager::instance()->defaultProfile();

    Q_ASSERT(sourceProfile);

    auto newProfile = Profile::Ptr(new Profile(ProfileManager::instance()->fallbackProfile()));
    newProfile->clone(sourceProfile, true);
    // TODO: add number suffix when the name is taken
    newProfile->setProperty(Profile::Name, i18nc("@item This will be used as part of the file name", "New Profile"));
    newProfile->setProperty(Profile::UntranslatedName, QStringLiteral("New Profile"));
    newProfile->setProperty(Profile::MenuIndex, QStringLiteral("0"));

    // Consider https://blogs.kde.org/2009/03/26/how-crash-almost-every-qtkde-application-and-how-fix-it-0 before changing the below
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
    const QList<Profile::Ptr> profiles = selectedProfiles();
    EditProfileDialog *profileDialog = nullptr;
    // sessions() returns a const QList
    for (const Session *session : SessionManager::instance()->sessions()) {
        const QList<TerminalDisplay *> viewsList = session->views();
        for (TerminalDisplay *terminalDisplay : viewsList) {
            // Searching for opened profiles
            profileDialog = terminalDisplay->sessionController()->profileDialogPointer();
            if (profileDialog != nullptr) {
                for (const Profile::Ptr &profile : profiles) {
                    if (profile->name() == profileDialog->lookupProfile()->name()
                        && profileDialog->isVisible()) {
                        // close opened edit dialog
                        profileDialog->close();
                    }
                }
            }
         }
    }

    EditProfileDialog dialog(this);
    // the dialog will delete the profile group when it is destroyed
    ProfileGroup* group = new ProfileGroup;
    for (const Profile::Ptr &profile : profiles) {
        group->addProfile(profile);
    }
    group->updateValues();

    dialog.setProfile(Profile::Ptr(group));
    dialog.exec();
}
QList<Profile::Ptr> ProfileSettings::selectedProfiles() const
{
    QList<Profile::Ptr> list;
    QItemSelectionModel* selection = profilesList->selectionModel();
    if (selection == nullptr) {
        return list;
    }

    const QList<QModelIndex> selectedIndexes = selection->selectedIndexes();
    for (const QModelIndex &index : selectedIndexes) {
        if (index.column() == ProfileColumn) {
            list << index.data(ProfilePtrRole).value<Profile::Ptr>();
        }
    }

    return list;
}
Profile::Ptr ProfileSettings::currentProfile() const
{
    QItemSelectionModel* selection = profilesList->selectionModel();

    if ((selection == nullptr) || selection->selectedRows().count() != 1) {
        return Profile::Ptr();
    }

    return selection->
           selectedIndexes().at(ProfileColumn).data(ProfilePtrRole).value<Profile::Ptr>();
}
bool ProfileSettings::isProfileDeletable(Profile::Ptr profile) const
{
    if (!profile) {
        return false;
    }

    const QFileInfo fileInfo(profile->path());
    if (!fileInfo.exists()) {
        return false;
    }

    const QFileInfo dirInfo(fileInfo.path());
    return dirInfo.isWritable();
}
void ProfileSettings::updateFavoriteStatus(const Profile::Ptr &profile, bool favorite)
{
    Q_ASSERT(_sessionModel);

    const int rowCount = _sessionModel->rowCount();
    for (int i = 0; i < rowCount; i++) {
        auto *item = _sessionModel->item(i, ProfileColumn);
        if (item->data(ProfilePtrRole).value<Profile::Ptr>() == profile) {
            auto *favoriteItem = _sessionModel->item(i, FavoriteStatusColumn);
            favoriteItem->setCheckState(favorite ? Qt::Checked : Qt::Unchecked);
            break;
        }
    }
}
void ProfileSettings::setShortcutEditorVisible(bool visible)
{
    profilesList->setColumnHidden(ShortcutColumn, !visible);
}
void StyledBackgroundPainter::drawBackground(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex&)
{
    const auto* opt = qstyleoption_cast<const QStyleOptionViewItem*>(&option);
    const QWidget* widget = opt != nullptr ? opt->widget : nullptr;

    QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, widget);
}

ShortcutItemDelegate::ShortcutItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent),
    _modifiedEditors(QSet<QWidget *>()),
    _itemsBeingEdited(QSet<QModelIndex>())
{
}
void ShortcutItemDelegate::editorModified()
{
    auto* editor = qobject_cast<FilteredKeySequenceEdit*>(sender());
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

    QString shortcut = qobject_cast<FilteredKeySequenceEdit *>(editor)->keySequence().toString();
    model->setData(index, shortcut, Qt::DisplayRole);

    _modifiedEditors.remove(editor);
}

QWidget* ShortcutItemDelegate::createEditor(QWidget* aParent, const QStyleOptionViewItem&, const QModelIndex& index) const
{
    _itemsBeingEdited.insert(index);

    auto editor = new FilteredKeySequenceEdit(aParent);
    QString shortcutString = index.data(Qt::DisplayRole).toString();
    editor->setKeySequence(QKeySequence::fromString(shortcutString));
    connect(editor, &QKeySequenceEdit::editingFinished, this, &Konsole::ShortcutItemDelegate::editorModified);
    editor->setFocus(Qt::FocusReason::MouseFocusReason);
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

QSize Konsole::ShortcutItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const QString shortcutString = index.data(Qt::DisplayRole).toString();
    QFontMetrics fm = option.fontMetrics;

    static const int editorMargins = 16; // chosen empirically
    const int width = fm.boundingRect(shortcutString + QStringLiteral(", ...")).width()
                      + editorMargins;

    return {width, QStyledItemDelegate::sizeHint(option, index).height()};
}

void Konsole::ShortcutItemDelegate::destroyEditor(QWidget *editor, const QModelIndex &index) const
{
    _itemsBeingEdited.remove(index);
    _modifiedEditors.remove(editor);
    editor->deleteLater();
}

void Konsole::FilteredKeySequenceEdit::keyPressEvent(QKeyEvent *event)
{
    if(event->modifiers() == Qt::NoModifier) {
        switch(event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            emit editingFinished();
            return;
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            clear();
            emit editingFinished();
            event->accept();
            return;
        default:
            event->accept();
            return;
        }
    }
    QKeySequenceEdit::keyPressEvent(event);
}
