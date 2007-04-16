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

// Qt
#include <QCheckBox>
#include <QHeaderView>
#include <QItemDelegate>
#include <QItemEditorFactory>
#include <QMetaProperty>
#include <QStandardItemEditorCreator>
#include <QScrollBar>
#include <QStandardItemModel>

// Konsole
#include "EditSessionDialog.h"
#include "SessionManager.h"
#include "ui_SessionTypeDialog.h"
#include "SessionTypeDialog.h"

using namespace Konsole;

SessionTypeDialog::SessionTypeDialog(QWidget* parent)
    : KDialog(parent)
{
    setCaption("Sessions");

    _ui = new Ui::SessionTypeDialog();
    _ui->setupUi(mainWidget());

    // hide vertical header
    _ui->sessionTable->verticalHeader()->hide();
    _ui->sessionTable->setItemDelegateForColumn(1,new SessionViewDelegate(this));

    // update table and listen for changes to the session types
    updateTableModel();
    connect( SessionManager::instance() , SIGNAL(sessionTypeAdded(const QString&)) , this,
             SLOT(updateTableModel()) );
    connect( SessionManager::instance() , SIGNAL(sessionTypeRemoved(const QString&)) , this,
             SLOT(updateTableModel()) );

    // ensure that session names are fully visible
    _ui->sessionTable->resizeColumnToContents(0);
    _ui->sessionTable->resizeColumnToContents(1);

    // resize the session table to the full width of the table
    _ui->sessionTable->horizontalHeader()->setStretchLastSection(true);
    _ui->sessionTable->horizontalHeader()->setHighlightSections(false);
    
    // setup buttons
    connect( _ui->newSessionButton , SIGNAL(clicked()) , this , SLOT(newType()) );
    connect( _ui->editSessionButton , SIGNAL(clicked()) , this , SLOT(editSelected()) );
    connect( _ui->deleteSessionButton , SIGNAL(clicked()) , this , SLOT(deleteSelected()) );
    connect( _ui->setAsDefaultButton , SIGNAL(clicked()) , this , SLOT(setSelectedAsDefault()) );

}
SessionTypeDialog::~SessionTypeDialog()
{
    delete _ui;
}
void SessionTypeDialog::updateTableModel()
{
    // setup session table
    _sessionModel = new QStandardItemModel(this);
    _sessionModel->setHorizontalHeaderLabels( QStringList() << "Name"
                                                            << "Show in Menu" );
    QListIterator<QString> keyIter( SessionManager::instance()->availableSessionTypes() );
    while ( keyIter.hasNext() )
    {
        const QString& key = keyIter.next();

        SessionInfo* info = SessionManager::instance()->sessionType(key);

        QList<QStandardItem*> itemList;
        QStandardItem* item = new QStandardItem( info->name() );
        item->setData(key);
        
        const bool isFavorite = SessionManager::instance()->favorites().contains(key);

        QStandardItem* favoriteItem = new QStandardItem();
        if ( isFavorite )
            favoriteItem->setData(KIcon("favorites"),Qt::DecorationRole);
        else
            favoriteItem->setData(KIcon(),Qt::DecorationRole);

        favoriteItem->setData(key,Qt::UserRole+1);

        itemList << item << favoriteItem;

        _sessionModel->appendRow(itemList);
    }
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
void SessionTypeDialog::tableSelectionChanged(const QItemSelection& selection)
{
    bool enable = !selection.indexes().isEmpty();
    const SessionManager* manager = SessionManager::instance();
    const bool isNotDefault = enable && selectedKey() != manager->defaultSessionKey();

    _ui->editSessionButton->setEnabled(enable);
    // do not allow the default session type to be removed
    _ui->deleteSessionButton->setEnabled(isNotDefault);
    _ui->setAsDefaultButton->setEnabled(isNotDefault); 
}
void SessionTypeDialog::deleteSelected()
{
    Q_ASSERT( selectedKey() != SessionManager::instance()->defaultSessionKey() );

    SessionManager::instance()->deleteSessionType(selectedKey());
}
void SessionTypeDialog::setSelectedAsDefault()
{
    SessionManager::instance()->setDefaultSessionType(selectedKey());
    // do not allow the new default session type to be removed
    _ui->deleteSessionButton->setEnabled(false);
    _ui->setAsDefaultButton->setEnabled(false); 
}
void SessionTypeDialog::newType()
{
    EditSessionDialog dialog(this);
    // base new type off the default session type
    dialog.setSessionType(QString()); 
    dialog.exec(); 
}
void SessionTypeDialog::editSelected()
{
    EditSessionDialog dialog(this);
    dialog.setSessionType(selectedKey());
    dialog.exec();
}
QString SessionTypeDialog::selectedKey() const
{
    Q_ASSERT( _ui->sessionTable->selectionModel() &&
              _ui->sessionTable->selectionModel()->selectedRows().count() == 1 );
    // TODO There has to be an easier way of getting the data
    // associated with the currently selected item
    return _ui->sessionTable->
            selectionModel()->
            selectedIndexes().first().data( Qt::UserRole + 1 ).value<QString>();
}

SessionViewDelegate::SessionViewDelegate(QObject* parent)
    : QItemDelegate(parent)
{
}
bool SessionViewDelegate::editorEvent(QEvent* event,QAbstractItemModel* model,
                                    const QStyleOptionViewItem&,const QModelIndex& index)
{
     if ( event->type() == QEvent::MouseButtonPress || event->type() == QEvent::KeyPress )
     {
         const QString& key = index.data(Qt::UserRole + 1).value<QString>();
         const bool isFavorite = !SessionManager::instance()->favorites().contains(key);
                                                
        SessionManager::instance()->setFavorite(key,
                                            isFavorite);

         if ( isFavorite )
            model->setData(index,KIcon("favorites"),Qt::DecorationRole);
         else
            model->setData(index,KIcon(),Qt::DecorationRole);
     }
     
     return true; 
}

#include "SessionTypeDialog.moc"
