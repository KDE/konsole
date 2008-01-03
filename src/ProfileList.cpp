/*
    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>

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
#include "ProfileList.h"

// Qt
#include <QtGui/QAction>
#include <QtGui/QActionGroup>
#include <KDebug>

// KDE
#include <KIcon>
#include <KLocalizedString>

// Konsole
#include "SessionManager.h"

using namespace Konsole;

ProfileList::ProfileList(bool addShortcuts , QObject* parent)
    : QObject(parent)
    , _addShortcuts(addShortcuts)
    , _emptyListAction(0)
{
    SessionManager* manager = SessionManager::instance();

    // construct the list of favorite session types
    _group = new QActionGroup(this);
    
    // disabled action to be shown only when the list is empty
    _emptyListAction = new QAction(i18n("No profiles available"),_group);
    _emptyListAction->setEnabled(false);
    

    QList<QString> list = manager->findFavorites().toList();
    qSort(list);
    QListIterator<QString> iter(list);

    while (iter.hasNext())
    {
        const QString& key = iter.next();
        favoriteChanged(key,true);        
    }

    connect( _group , SIGNAL(triggered(QAction*)) , this , SLOT(triggered(QAction*)) );


    // listen for future changes to the session list
    connect( manager , SIGNAL(favoriteStatusChanged(const QString&,bool)) , this ,
             SLOT(favoriteChanged(const QString&,bool)) );
	connect( manager , SIGNAL(shortcutChanged(QString,QKeySequence)) , this , 
			 SLOT(shortcutChanged(QString,QKeySequence)) );
    connect( manager , SIGNAL(profileChanged(const QString&)) , this , 
             SLOT(profileChanged(const QString&)) );
}
void ProfileList::updateEmptyAction() 
{
    Q_ASSERT( _group );
    Q_ASSERT( _emptyListAction );

    // show empty list action when it is the only action
    // in the group
    const bool showEmptyAction = _group->actions().count() == 1;

    if ( showEmptyAction != _emptyListAction->isVisible() )
        _emptyListAction->setVisible(showEmptyAction);
}
QAction* ProfileList::actionForKey(const QString& key) const
{        
    QListIterator<QAction*> iter(_group->actions());
    while ( iter.hasNext() )
    {
        QAction* next = iter.next();
        if ( next->data() == key )
            return next;
    }
    return 0; // not found
}

void ProfileList::profileChanged(const QString& key)
{
    QAction* action = actionForKey(key);
    if ( action )
        updateAction(action,SessionManager::instance()->profile(key));
}

void ProfileList::updateAction(QAction* action , Profile* info)
{
    Q_ASSERT(action);
    Q_ASSERT(info);

    action->setText(info->name());
    action->setIcon(KIcon(info->icon()));
}
void ProfileList::shortcutChanged(const QString& key,const QKeySequence& sequence)
{
	if ( !_addShortcuts )
		return;

	QAction* action = actionForKey(key);

	if ( action )
	{
		action->setShortcut(sequence);
	}
}
void ProfileList::favoriteChanged(const QString& key,bool isFavorite)
{
    SessionManager* manager = SessionManager::instance();

    if ( isFavorite )
    {
        Profile* info = manager->profile(key);

        QAction* action = new QAction(_group);
        action->setData( key );
        
        if ( _addShortcuts )
        {
            action->setShortcut(manager->shortcut(key));
        }

        updateAction(action,info);

        emit actionsChanged(_group->actions());
    }
    else
    {
        QAction* action = actionForKey(key);

        if ( action )
        {
            _group->removeAction(action);
            emit actionsChanged(_group->actions());
        }
    }

    updateEmptyAction();
}

void ProfileList::triggered(QAction* action)
{
    // assert that session key is still valid
    Q_ASSERT( SessionManager::instance()->profile( action->data().toString() ) );

    emit profileSelected( action->data().toString() );
}

QList<QAction*> ProfileList::actions()
{
    return _group->actions();
}

#include "ProfileList.moc"
