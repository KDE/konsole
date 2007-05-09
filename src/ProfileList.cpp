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
#include <QtCore/QDebug>

// KDE
#include <KIcon>

// Konsole
#include "SessionManager.h"

using namespace Konsole;

ProfileList::ProfileList(QObject* parent)
    : QObject(parent)
{
    SessionManager* manager = SessionManager::instance();

    // construct the list of favorite session types
    _group = new QActionGroup(this);

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
    connect( manager , SIGNAL(profileChanged(const QString&)) , this , 
             SLOT(profileChanged(const QString&)) );
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

void ProfileList::favoriteChanged(const QString& key,bool isFavorite)
{
    if ( isFavorite )
    {
        Profile* info = SessionManager::instance()->profile(key);

        QAction* action = new QAction(_group);
        action->setData( key );

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
