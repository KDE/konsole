/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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

#include <KDebug>

// Konsole
#include "EditProfileDialog.h"
#include "SessionManager.h"
#include "ui_ManageProfilesDialog.h"

using namespace Konsole;

ManageProfilesDialog::ManageProfilesDialog(QWidget* parent)
    : KDialog(parent)
{
    setCaption(i18n("Manage Profiles"));
    setButtons( KDialog::Close ); 

    _ui = new Ui::ManageProfilesDialog();
    _ui->setupUi(mainWidget());

    // hide vertical header
    _ui->sessionTable->verticalHeader()->hide();
    _ui->sessionTable->setItemDelegateForColumn(FavoriteStatusColumn,new ProfileItemDelegate(this));

    // update table and listen for changes to the session types
    updateTableModel();
    connect( SessionManager::instance() , SIGNAL(profileAdded(const QString&)) , this,
             SLOT(updateTableModel()) );
    connect( SessionManager::instance() , SIGNAL(profileRemoved(const QString&)) , this,
             SLOT(updateTableModel()) );
    connect( SessionManager::instance() , SIGNAL(profileChanged(const QString&)) , this,
             SLOT(updateTableModel()) );
    connect( SessionManager::instance() , 
                SIGNAL(favoriteStatusChanged(const QString&,bool)) , this ,
                SLOT(updateFavoriteStatus(const QString&,bool)) );

    // resize the session table to the full width of the table
    _ui->sessionTable->horizontalHeader()->setHighlightSections(false);

    _ui->sessionTable->resizeColumnsToContents();

    // setup buttons
    connect( _ui->newSessionButton , SIGNAL(clicked()) , this , SLOT(newType()) );
    connect( _ui->editSessionButton , SIGNAL(clicked()) , this , SLOT(editSelected()) );
    connect( _ui->deleteSessionButton , SIGNAL(clicked()) , this , SLOT(deleteSelected()) );
    connect( _ui->setAsDefaultButton , SIGNAL(clicked()) , this , SLOT(setSelectedAsDefault()) );

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

        kDebug() << "New key sequence: " << item->text(); 

        SessionManager::instance()->setShortcut(item->data(ShortcutRole).value<QString>(),
                                                sequence); 
   } 
}
void ManageProfilesDialog::updateTableModel()
{
    // ensure profiles list is complete
    // this may be EXPENSIVE, but will only be done the first time
    // that the dialog is shown. 
    SessionManager::instance()->loadAllProfiles();

    // setup session table
    _sessionModel = new QStandardItemModel(this);
    _sessionModel->setHorizontalHeaderLabels( QStringList() << i18n("Name")
                                                            << i18n("Show in Menu") 
                                                            << i18n("Shortcut") );
    QListIterator<QString> keyIter( SessionManager::instance()->loadedProfiles() );
    while ( keyIter.hasNext() )
    {
        const QString& key = keyIter.next();

        Profile* info = SessionManager::instance()->profile(key);

        if ( info->isHidden() )
            continue;

        QList<QStandardItem*> itemList;
        QStandardItem* item = new QStandardItem( info->name() );

        if ( !info->icon().isEmpty() )
            item->setIcon( KIcon(info->icon()) );
        item->setData(key,ProfileKeyRole);

        const bool isFavorite = SessionManager::instance()->findFavorites().contains(key);

        // favorite column
        QStandardItem* favoriteItem = new QStandardItem();
        if ( isFavorite )
            favoriteItem->setData(KIcon("favorites"),Qt::DecorationRole);
        else
            favoriteItem->setData(KIcon(),Qt::DecorationRole);

        favoriteItem->setData(key,ProfileKeyRole);

        // shortcut column
        QStandardItem* shortcutItem = new QStandardItem();
        QString shortcut = SessionManager::instance()->shortcut(key).
                                toString();
        shortcutItem->setText(shortcut);
        shortcutItem->setData(key,ShortcutRole);

        itemList << item << favoriteItem << shortcutItem;

        _sessionModel->appendRow(itemList);
    }
    updateDefaultItem();

    connect( _sessionModel , SIGNAL(itemChanged(QStandardItem*)) , this , 
            SLOT(itemDataChanged(QStandardItem*)) );

    _ui->sessionTable->setModel(_sessionModel);

    // listen for changes in the table selection and update the state of the form's buttons
    // accordingly.
    //
    // it appears that the selection model is changed when the model itself is replaced,
    // so the signals need to be reconnected each time the model is updated.
    //
    // the view ( _ui->sessionTable ) has a selectionChanged() signal which it would be
    // preferable to connect to instead of the one belonging to the view's current 
    // selection model, but QAbstractItemView::selectionChanged() is protected
    //
    connect( _ui->sessionTable->selectionModel() , 
            SIGNAL(selectionChanged(const QItemSelection&,const QItemSelection&)) , this ,
            SLOT(tableSelectionChanged(const QItemSelection&)) );

    tableSelectionChanged( _ui->sessionTable->selectionModel()->selection() );

}
void ManageProfilesDialog::updateDefaultItem()
{
    const QString& defaultKey = SessionManager::instance()->defaultProfileKey();

    for ( int i = 0 ; i < _sessionModel->rowCount() ; i++ )
    {
        QStandardItem* item = _sessionModel->item(i);
        QFont font = item->font();

        bool isDefault = ( defaultKey == item->data().value<QString>() );

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
void ManageProfilesDialog::tableSelectionChanged(const QItemSelection& selection)
{
    bool enable = !selection.indexes().isEmpty();
    const SessionManager* manager = SessionManager::instance();
    const bool isNotDefault = enable && selectedKey() != manager->defaultProfileKey();

    _ui->editSessionButton->setEnabled(enable);
    // do not allow the default session type to be removed
    _ui->deleteSessionButton->setEnabled(isNotDefault);
    _ui->setAsDefaultButton->setEnabled(isNotDefault); 
}
void ManageProfilesDialog::deleteSelected()
{
	// TODO - Possibly add a warning here if the selected profile is the parent of 
	// one or more other profiles since deleting a profile will change the settings
	// of the profiles which inherit from it.
	
	Q_ASSERT( !selectedKey().isEmpty()  );
    Q_ASSERT( selectedKey() != SessionManager::instance()->defaultProfileKey() );

    SessionManager::instance()->deleteProfile(selectedKey());
}
void ManageProfilesDialog::setSelectedAsDefault()
{
    SessionManager::instance()->setDefaultProfile(selectedKey());
    // do not allow the new default session type to be removed
    _ui->deleteSessionButton->setEnabled(false);
    _ui->setAsDefaultButton->setEnabled(false);

    // update font of new default item
    updateDefaultItem(); 
}
void ManageProfilesDialog::newType()
{
    EditProfileDialog dialog(this);
 
    // setup a temporary profile, inheriting from the selected profile
	// or the default if no profile is selected
    Profile* parentProfile = 0;

	QString selectedProfileKey = selectedKey();
	if ( selectedProfileKey.isEmpty() ) 
		parentProfile = SessionManager::instance()->defaultProfile();
	else
		parentProfile = SessionManager::instance()->profile(selectedProfileKey);

	Q_ASSERT( parentProfile );

    Profile* newProfile = new Profile(parentProfile);
    newProfile->setProperty(Profile::Name,i18n("New Profile"));
    const QString& key = SessionManager::instance()->addProfile(newProfile);

	kDebug() << "Key for new profile" << key;

    dialog.setProfile(key); 
    dialog.selectProfileName();

    // if the user doesn't accept the dialog, remove the temporary profile
    // if they do accept the dialog, it will become a permanent profile
    if ( dialog.exec() != QDialog::Accepted )
        SessionManager::instance()->deleteProfile(key);
    else
    {
        SessionManager::instance()->setFavorite(key,true);
    }
}
void ManageProfilesDialog::editSelected()
{
	Q_ASSERT( !selectedKey().isEmpty() );

    EditProfileDialog dialog(this);
    dialog.setProfile(selectedKey());
    dialog.exec();
}
QString ManageProfilesDialog::selectedKey() const
{
	QItemSelectionModel* selection = _ui->sessionTable->selectionModel();

	if ( !selection || selection->selectedRows().count() != 1 )
		return QString();

    // TODO There has to be an easier way of getting the data
    // associated with the currently selected item
    return  selection->
            selectedIndexes().first().data( Qt::UserRole + 1 ).value<QString>();
}
void ManageProfilesDialog::updateFavoriteStatus(const QString& key , bool favorite)
{
    Q_ASSERT( _sessionModel );

    const QModelIndex topIndex = _sessionModel->index(0,FavoriteStatusColumn);

    QModelIndexList list = _sessionModel->match( topIndex , ProfileKeyRole,
                                                 key );

    foreach( QModelIndex index , list )
    {
        const KIcon icon = favorite ? KIcon("favorites") : KIcon();
        _sessionModel->setData(index,icon,Qt::DecorationRole);
    }
}
void ManageProfilesDialog::setShortcutEditorVisible(bool visible)
{
	_ui->sessionTable->setColumnHidden(ShortcutColumn,!visible);	
}
ProfileItemDelegate::ProfileItemDelegate(QObject* parent)
    : QItemDelegate(parent)
{
}
// Is there a simpler way of centering the decoration than re-implementing
// drawDecoration?
void ProfileItemDelegate::drawDecoration(QPainter* painter,
                                         const QStyleOptionViewItem& option,
                                         const QRect& rect,const QPixmap& pixmap) const
{
    QStyleOptionViewItem centeredOption(option);
    centeredOption.decorationAlignment = Qt::AlignCenter;
    QItemDelegate::drawDecoration(painter,
                                  centeredOption,
                                  QStyle::alignedRect(Qt::LeftToRight,Qt::AlignCenter,rect.size(),option.rect),
                                  pixmap);
}
bool ProfileItemDelegate::editorEvent(QEvent* event,QAbstractItemModel*,
                                    const QStyleOptionViewItem&,const QModelIndex& index)
{
     if ( event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress )
     {
         const QString& key = index.data(ManageProfilesDialog::ProfileKeyRole).value<QString>();
         const bool isFavorite = !SessionManager::instance()->findFavorites().contains(key);
                                                
        SessionManager::instance()->setFavorite(key,
                                            isFavorite);
     }
     
     return true; 
}

#include "ManageProfilesDialog.moc"
