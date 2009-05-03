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
#include <QtGui/QCheckBox>
#include <QtGui/QHeaderView>
#include <QtGui/QItemDelegate>
#include <QtGui/QItemEditorCreator>
#include <QtCore/QMetaEnum>
#include <QtGui/QScrollBar>
#include <QtGui/QShowEvent>
#include <QtGui/QStandardItem>

// KDE
#include <KKeySequenceWidget>
#include <KDebug>

// Konsole
#include "EditProfileDialog.h"
#include "SessionManager.h"
#include "ui_ManageProfilesDialog.h"

using namespace Konsole;

ManageProfilesDialog::ManageProfilesDialog(QWidget* parent)
    : KDialog(parent)
    , _sessionModel(new QStandardItemModel(this))
{
    setCaption(i18n("Manage Profiles"));
    setButtons( KDialog::Ok | KDialog::Apply | KDialog::Cancel ); 

    connect( this, SIGNAL(applyClicked()) , this , SLOT(setMenuOrder()) );

    _ui = new Ui::ManageProfilesDialog();
    _ui->setupUi(mainWidget());

    // hide vertical header
    _ui->sessionTable->verticalHeader()->hide();
    _ui->sessionTable->setItemDelegateForColumn(FavoriteStatusColumn,new FavoriteItemDelegate(this));
    _ui->sessionTable->setItemDelegateForColumn(ShortcutColumn,new ShortcutItemDelegate(this));
    _ui->sessionTable->setEditTriggers(_ui->sessionTable->editTriggers() | QAbstractItemView::SelectedClicked);

    // TODO re-enable when saving profile order works - khindenburg
    _ui->moveUpButton->setEnabled(false);
    _ui->moveDownButton->setEnabled(false);

    // update table and listen for changes to the session types
    connect( SessionManager::instance() , SIGNAL(profileAdded(Profile::Ptr)) , this,
             SLOT(addItems(Profile::Ptr)) );
    connect( SessionManager::instance() , SIGNAL(profileRemoved(Profile::Ptr)) , this,
             SLOT(removeItems(Profile::Ptr)) );
    connect( SessionManager::instance() , SIGNAL(profileChanged(Profile::Ptr)) , this,
             SLOT(updateItems(Profile::Ptr)) );
    connect( SessionManager::instance() , 
                SIGNAL(favoriteStatusChanged(Profile::Ptr,bool)) , this ,
                SLOT(updateFavoriteStatus(Profile::Ptr,bool)) );
    populateTable();
    
    // resize the session table to the full width of the table
    _ui->sessionTable->horizontalHeader()->setHighlightSections(false);
    _ui->sessionTable->resizeColumnsToContents();
    
    // allow a larger width for the shortcut column to account for the 
    // increased with needed by the shortcut editor compared with just
    // displaying the text of the shortcut
    _ui->sessionTable->setColumnWidth(ShortcutColumn,
                _ui->sessionTable->columnWidth(ShortcutColumn)+100);

    // setup buttons
    connect( _ui->newSessionButton , SIGNAL(clicked()) , this , SLOT(newType()) );
    connect( _ui->editSessionButton , SIGNAL(clicked()) , this , SLOT(editSelected()) );
    connect( _ui->deleteSessionButton , SIGNAL(clicked()) , this , SLOT(deleteSelected()) );
    connect( _ui->setAsDefaultButton , SIGNAL(clicked()) , this , SLOT(setSelectedAsDefault()) );
    connect( _ui->moveUpButton , SIGNAL(clicked()) , this , SLOT(moveUpSelected()) );
    connect( _ui->moveDownButton , SIGNAL(clicked()) , this , SLOT(moveDownSelected()) );
}

void ManageProfilesDialog::showEvent(QShowEvent*)
{
    Q_ASSERT( _ui->sessionTable->model() );

    // try to ensure that all the text in all the columns is visible initially.
    // FIXME:  this is not a good solution, look for a more correct way to do this

    int totalWidth = 0;
    int columnCount = _ui->sessionTable->model()->columnCount();

    for ( int i = 0 ; i < columnCount ; i++ )
        totalWidth += _ui->sessionTable->columnWidth(i);

    // the margin is added to account for the space taken by the resize grips
    // between the columns, this ensures that a horizontal scroll bar is not added 
    // automatically
    int margin = style()->pixelMetric( QStyle::PM_HeaderGripMargin ) * columnCount;
    _ui->sessionTable->setMinimumWidth( totalWidth + margin );
    _ui->sessionTable->horizontalHeader()->setStretchLastSection(true);
}

ManageProfilesDialog::~ManageProfilesDialog()
{
    delete _ui;
}
void ManageProfilesDialog::itemDataChanged(QStandardItem* item)
{
   if ( item->column() == ShortcutColumn )
   {
        QKeySequence sequence = QKeySequence::fromString(item->text());
        SessionManager::instance()->setShortcut(item->data(ShortcutRole).value<Profile::Ptr>(),
                                                sequence); 
   } 
}

void ManageProfilesDialog::setMenuOrder(void)
{
    return;
// TODO fix 
/*
    for (int i=0;i<_sessionModel->rowCount();i++)
    {
    }

    SessionManager::instance()->setMenuOrder();
*/
}

int ManageProfilesDialog::rowForProfile(const Profile::Ptr info) const
{
    for (int i=0;i<_sessionModel->rowCount();i++)
    {
        if (_sessionModel->item(i,ProfileNameColumn)->data(ProfileKeyRole)
            .value<Profile::Ptr>() == info)
        {
            return i;
        }
    }
    return -1;
}
void ManageProfilesDialog::removeItems(const Profile::Ptr info)
{
    int row = rowForProfile(info);
    if (row < 0)
        return;
    _sessionModel->removeRow(row);
}
void ManageProfilesDialog::updateItems(const Profile::Ptr info)
{
    int row = rowForProfile(info);
    if (row < 0)
        return;

    QList<QStandardItem*> items;
    items << _sessionModel->item(row,ProfileNameColumn);
    items << _sessionModel->item(row,FavoriteStatusColumn);
    items << _sessionModel->item(row,ShortcutColumn);
    updateItemsForProfile(info,items);
}
void ManageProfilesDialog::updateItemsForProfile(const Profile::Ptr info, QList<QStandardItem*>& items) const
{
    // Profile Name
    items[ProfileNameColumn]->setText(info->name());
    if ( !info->icon().isEmpty() )
       items[ProfileNameColumn]->setIcon( KIcon(info->icon()) );

    items[ProfileNameColumn]->setData(QVariant::fromValue(info),ProfileKeyRole);

    // Favorite Status
    const bool isFavorite = SessionManager::instance()->findFavorites().contains(info);
    if ( isFavorite )
       items[FavoriteStatusColumn]->setData(KIcon("dialog-ok-apply"),Qt::DecorationRole);
    else
       items[FavoriteStatusColumn]->setData(KIcon(),Qt::DecorationRole);
    items[FavoriteStatusColumn]->setData(QVariant::fromValue(info),ProfileKeyRole);

    // Shortcut
    QString shortcut = SessionManager::instance()->shortcut(info).
                           toString();
    items[ShortcutColumn]->setText(shortcut);
    items[ShortcutColumn]->setData(QVariant::fromValue(info),ShortcutRole);
}
void ManageProfilesDialog::addItems(const Profile::Ptr profile) 
{
    if (profile->isHidden())
        return;

    QList<QStandardItem*> items;
    for (int i=0;i<3;i++)
        items << new QStandardItem;
    updateItemsForProfile(profile,items);
    _sessionModel->appendRow(items);
}
void ManageProfilesDialog::populateTable()
{
    Q_ASSERT(!_ui->sessionTable->model());
    
    _ui->sessionTable->setModel(_sessionModel);
    // ensure profiles list is complete
    // this may be expensive, but will only be done the first time
    // that the dialog is shown. 
    SessionManager::instance()->loadAllProfiles();

    _sessionModel->clear();
    // setup session table
    _sessionModel->setHorizontalHeaderLabels( QStringList() << i18n("Name")
                                                            << i18n("Show in Menu") 
                                                            << i18n("Shortcut") );

    QList<Profile::Ptr> profiles = SessionManager::instance()->loadedProfiles();
    SessionManager::instance()->sortProfiles(profiles);

    foreach(const Profile::Ptr &info, profiles)
    {
        addItems(info);
    }
    updateDefaultItem();

    connect( _sessionModel , SIGNAL(itemChanged(QStandardItem*)) , this , 
            SLOT(itemDataChanged(QStandardItem*)) );

    // listen for changes in the table selection and update the state of the form's buttons
    // accordingly.
    //
    // it appears that the selection model is changed when the model itself is replaced,
    // so the signals need to be reconnected each time the model is updated.
    connect( _ui->sessionTable->selectionModel() , 
            SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)) , this ,
            SLOT(tableSelectionChanged(const QItemSelection&)) );

    _ui->sessionTable->selectRow(0);
    tableSelectionChanged( _ui->sessionTable->selectionModel()->selection() );
}
void ManageProfilesDialog::updateDefaultItem()
{
    Profile::Ptr defaultProfile = SessionManager::instance()->defaultProfile();

    for ( int i = 0 ; i < _sessionModel->rowCount() ; i++ )
    {
        QStandardItem* item = _sessionModel->item(i);
        QFont font = item->font();

        bool isDefault = ( defaultProfile == item->data().value<Profile::Ptr>() );

        if ( isDefault && !font.bold() )
        {
            font.setBold(true);
            item->setFont(font);
        } 
        else if ( !isDefault && font.bold() )
        {
            font.setBold(false);
            item->setFont(font);
        } 
    }
}
void ManageProfilesDialog::tableSelectionChanged(const QItemSelection&)
{
    const int selectedRows = _ui->sessionTable->selectionModel()->selectedRows().count();
    const SessionManager* manager = SessionManager::instance();
    const bool isNotDefault = (selectedRows > 0) && currentProfile() != manager->defaultProfile();
    const int rowIndex = _ui->sessionTable->currentIndex().row();

    _ui->newSessionButton->setEnabled(selectedRows < 2);
    _ui->editSessionButton->setEnabled(selectedRows > 0);
    // do not allow the default session type to be removed
    _ui->deleteSessionButton->setEnabled(isNotDefault);
    _ui->setAsDefaultButton->setEnabled(isNotDefault && (selectedRows < 2)); 

    // TODO handle multiple moves
    // TODO re-enable when saving profile order works - khindenburg
//    _ui->moveUpButton->setEnabled((selectedRows == 1) && (rowIndex > 0));
//    _ui->moveDownButton->setEnabled((selectedRows == 1) && (rowIndex < (_sessionModel->rowCount()-1)));

    _ui->sessionTable->selectRow(rowIndex);
}
void ManageProfilesDialog::deleteSelected()
{
    foreach(const Profile::Ptr &profile, selectedProfiles())
    {
        if (profile != SessionManager::instance()->defaultProfile())
            SessionManager::instance()->deleteProfile(profile);
    }
}
void ManageProfilesDialog::setSelectedAsDefault()
{
    SessionManager::instance()->setDefaultProfile(currentProfile());
    // do not allow the new default session type to be removed
    _ui->deleteSessionButton->setEnabled(false);
    _ui->setAsDefaultButton->setEnabled(false);

    // update font of new default item
    updateDefaultItem(); 
}

void ManageProfilesDialog::moveUpSelected()
{
    Q_ASSERT(_sessionModel);

    const int rowIndex = _ui->sessionTable->currentIndex().row();
    const QList<QStandardItem*>items = _sessionModel->takeRow(rowIndex);
    _sessionModel->insertRow(rowIndex-1, items);
    _ui->sessionTable->selectRow(rowIndex-1);
}

void ManageProfilesDialog::moveDownSelected()
{
    Q_ASSERT(_sessionModel);

    const int rowIndex = _ui->sessionTable->currentIndex().row();
    const QList<QStandardItem*>items = _sessionModel->takeRow(rowIndex);
    _sessionModel->insertRow(rowIndex+1, items);
    _ui->sessionTable->selectRow(rowIndex+1);
}

void ManageProfilesDialog::newType()
{
    EditProfileDialog dialog(this);
 
    // setup a temporary profile which is a clone of the selected profile 
    // or the default if no profile is selected
    Profile::Ptr sourceProfile;
    
    Profile::Ptr selectedProfile = currentProfile();
    if ( !selectedProfile ) 
        sourceProfile = SessionManager::instance()->defaultProfile();
    else
        sourceProfile = selectedProfile; 

    Q_ASSERT( sourceProfile );

    Profile::Ptr newProfile = Profile::Ptr(new Profile(SessionManager::instance()->fallbackProfile()));
    newProfile->clone(sourceProfile,true);
    newProfile->setProperty(Profile::Name,i18n("New Profile"));
    newProfile->setProperty(Profile::MenuIndex, QString("0"));

    dialog.setProfile(newProfile); 
    dialog.selectProfileName();

    if ( dialog.exec() == QDialog::Accepted )
    {
        SessionManager::instance()->addProfile(newProfile);
        SessionManager::instance()->setFavorite(newProfile,true);
    }
}
void ManageProfilesDialog::editSelected()
{
    EditProfileDialog dialog(this);
    // the dialog will delete the profile group when it is destroyed
    ProfileGroup* group = new ProfileGroup;
    foreach(const Profile::Ptr &profile,selectedProfiles())
        group->addProfile(profile);
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

    foreach(const QModelIndex& index, selection->selectedIndexes())
    {
        if (index.column() == ProfileNameColumn)
            list << index.data(ProfileKeyRole).value<Profile::Ptr>();
    }

    return list;
}
Profile::Ptr ManageProfilesDialog::currentProfile() const
{
    QItemSelectionModel* selection = _ui->sessionTable->selectionModel();

    if ( !selection || selection->selectedRows().count() != 1 )
        return Profile::Ptr();

    return  selection->
            selectedIndexes().first().data(ProfileKeyRole).value<Profile::Ptr>();
}
void ManageProfilesDialog::updateFavoriteStatus(Profile::Ptr profile, bool favorite)
{
    Q_ASSERT( _sessionModel );

    int rowCount = _sessionModel->rowCount();
    for (int i=0;i < rowCount;i++)
    {
        QModelIndex index = _sessionModel->index(i,FavoriteStatusColumn);
        if (index.data(ProfileKeyRole).value<Profile::Ptr>() ==
            profile )
        {
            const KIcon icon = favorite ? KIcon("dialog-ok-apply") : KIcon();
            _sessionModel->setData(index,icon,Qt::DecorationRole);
        }
    }
}
void ManageProfilesDialog::setShortcutEditorVisible(bool visible)
{
    _ui->sessionTable->setColumnHidden(ShortcutColumn,!visible);    
}
void StyledBackgroundPainter::drawBackground(QPainter* painter, const QStyleOptionViewItem& option,
        const QModelIndex&)
{
    const QStyleOptionViewItemV3* v3option = qstyleoption_cast<const QStyleOptionViewItemV3*>(&option);
    const QWidget* widget = v3option ? v3option->widget : 0;

    QStyle* style = widget ? widget->style() : QApplication::style();
    
    style->drawPrimitive(QStyle::PE_PanelItemViewItem,&option,painter,widget);
}

FavoriteItemDelegate::FavoriteItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}
void FavoriteItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    // See implementation of QStyledItemDelegate::paint()
    QStyleOptionViewItemV4 opt = option;
    initStyleOption(&opt,index);

    StyledBackgroundPainter::drawBackground(painter,opt,index);

    int margin = (opt.rect.height()-opt.decorationSize.height())/2;
    margin++;

    opt.rect.setTop(opt.rect.top()+margin);
    opt.rect.setBottom(opt.rect.bottom()-margin);

    QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    icon.paint(painter,opt.rect,Qt::AlignCenter);
}

bool FavoriteItemDelegate::editorEvent(QEvent* event,QAbstractItemModel*,
                                    const QStyleOptionViewItem&,const QModelIndex& index)
{
     if ( event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress 
         || event->type() == QEvent::MouseButtonDblClick )
     {
         Profile::Ptr profile = index.data(ManageProfilesDialog::ProfileKeyRole).value<Profile::Ptr>();
         const bool isFavorite = !SessionManager::instance()->findFavorites().contains(profile);
                                             
        SessionManager::instance()->setFavorite(profile,
                                            isFavorite);
     }
     
     return true; 
}
ShortcutItemDelegate::ShortcutItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}
void ShortcutItemDelegate::editorModified(const QKeySequence& keys)
{
    kDebug() << keys.toString();

    KKeySequenceWidget* editor = qobject_cast<KKeySequenceWidget*>(sender());
    Q_ASSERT(editor);
    _modifiedEditors.insert(editor);
}
void ShortcutItemDelegate::setModelData(QWidget* editor, QAbstractItemModel* model,
                                    const QModelIndex& index) const
{
    _itemsBeingEdited.remove(index);
    
    if (!_modifiedEditors.contains(editor))
        return;

    QString shortcut = qobject_cast<KKeySequenceWidget*>(editor)->keySequence().toString();
    model->setData(index,shortcut,Qt::DisplayRole);

    _modifiedEditors.remove(editor);
}

QWidget* ShortcutItemDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex& index) const
{
    _itemsBeingEdited.insert(index);

    KKeySequenceWidget* editor = new KKeySequenceWidget(parent);
    editor->setFocusPolicy(Qt::StrongFocus);
    editor->setModifierlessAllowed(false);
    QString shortcutString = index.data(Qt::DisplayRole).toString();
    editor->setKeySequence(QKeySequence::fromString(shortcutString));
    connect(editor,SIGNAL(keySequenceChanged(QKeySequence)),this,SLOT(editorModified(QKeySequence)));
    editor->captureKeySequence();
    return editor;
}
void ShortcutItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, 
                        const QModelIndex& index) const
{
    if (_itemsBeingEdited.contains(index))
        StyledBackgroundPainter::drawBackground(painter,option,index);
    else
        QStyledItemDelegate::paint(painter,option,index);
}

#include "ManageProfilesDialog.moc"
