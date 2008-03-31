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
{
    setCaption(i18n("Manage Profiles"));
    setButtons( KDialog::Close ); 

    _ui = new Ui::ManageProfilesDialog();
    _ui->setupUi(mainWidget());

    // hide vertical header
    _ui->sessionTable->verticalHeader()->hide();
    _ui->sessionTable->setItemDelegateForColumn(FavoriteStatusColumn,new FavoriteItemDelegate(this));
	_ui->sessionTable->setItemDelegateForColumn(ShortcutColumn,new ShortcutItemDelegate(this));
	_ui->sessionTable->setEditTriggers(_ui->sessionTable->editTriggers() | QAbstractItemView::SelectedClicked);

    // update table and listen for changes to the session types
    updateTableModel();
    connect( SessionManager::instance() , SIGNAL(profileAdded(Profile::Ptr)) , this,
             SLOT(updateTableModel()) );
    connect( SessionManager::instance() , SIGNAL(profileRemoved(Profile::Ptr)) , this,
             SLOT(updateTableModel()) );
    connect( SessionManager::instance() , SIGNAL(profileChanged(Profile::Ptr)) , this,
             SLOT(updateTableModel()) );
    connect( SessionManager::instance() , 
                SIGNAL(favoriteStatusChanged(Profile::Ptr,bool)) , this ,
                SLOT(updateFavoriteStatus(Profile::Ptr,bool)) );

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

        SessionManager::instance()->setShortcut(item->data(ShortcutRole).value<Profile::Ptr>(),
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
    QListIterator<Profile::Ptr> profileIter( SessionManager::instance()->loadedProfiles() );
    while ( profileIter.hasNext() )
    {
        Profile::Ptr info = profileIter.next();

        if ( info->isHidden() )
            continue;

        QList<QStandardItem*> itemList;
        QStandardItem* item = new QStandardItem( info->name() );

        if ( !info->icon().isEmpty() )
            item->setIcon( KIcon(info->icon()) );
        
		item->setData(QVariant::fromValue(info),ProfileKeyRole);

        const bool isFavorite = SessionManager::instance()->findFavorites().contains(info);

        // favorite column
        QStandardItem* favoriteItem = new QStandardItem();
        if ( isFavorite )
            favoriteItem->setData(KIcon("favorites"),Qt::DecorationRole);
        else
            favoriteItem->setData(KIcon(),Qt::DecorationRole);

        favoriteItem->setData(QVariant::fromValue(info),ProfileKeyRole);

        // shortcut column
        QStandardItem* shortcutItem = new QStandardItem();
        QString shortcut = SessionManager::instance()->shortcut(info).
                                toString();
        shortcutItem->setText(shortcut);
        shortcutItem->setData(QVariant::fromValue(info),ShortcutRole);

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
void ManageProfilesDialog::tableSelectionChanged(const QItemSelection& selection)
{
    bool enable = !selection.indexes().isEmpty();
    const SessionManager* manager = SessionManager::instance();
    const bool isNotDefault = enable && selectedKey() != manager->defaultProfile();

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
	
	Q_ASSERT( selectedKey() );
    Q_ASSERT( selectedKey() != SessionManager::instance()->defaultProfile() );

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
    Profile::Ptr parentProfile;

	Profile::Ptr selectedProfile = selectedKey();
	if ( !selectedProfile ) 
		parentProfile = SessionManager::instance()->defaultProfile();
	else
		parentProfile = selectedProfile; 

	Q_ASSERT( parentProfile );

    Profile::Ptr newProfile = Profile::Ptr(new Profile(parentProfile));
    newProfile->setProperty(Profile::Name,i18n("New Profile"));

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
	Q_ASSERT( selectedKey() );

    EditProfileDialog dialog(this);
    dialog.setProfile(selectedKey());
    dialog.exec();
}
Profile::Ptr ManageProfilesDialog::selectedKey() const
{
	QItemSelectionModel* selection = _ui->sessionTable->selectionModel();

	if ( !selection || selection->selectedRows().count() != 1 )
		return Profile::Ptr();

    // TODO There has to be an easier way of getting the data
    // associated with the currently selected item
    return  selection->
            selectedIndexes().first().data( Qt::UserRole + 1 ).value<Profile::Ptr>();
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
			const KIcon icon = favorite ? KIcon("favorites") : KIcon();
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
