// Qt
#include <QAction>
#include <QActionGroup>
#include <QtDebug>

// KDE
#include <KIcon>

// Konsole
#include "ProfileList.h"
#include "SessionManager.h"

using namespace Konsole;

ProfileList::ProfileList(QObject* parent)
    : QObject(parent)
{
    SessionManager* manager = SessionManager::instance();

    // construct the list of favorite session types
    _group = new QActionGroup(this);

    QList<QString> list = manager->favorites().toList();
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
