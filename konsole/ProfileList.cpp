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
}

void ProfileList::favoriteChanged(const QString& key,bool isFavorite)
{
    if ( isFavorite )
    {
        Profile* info = SessionManager::instance()->sessionType(key);

        QAction* action = _group->addAction(info->name());
        action->setIcon( KIcon(info->icon()) );
        action->setData( key );

        emit actionsChanged(_group->actions());
    }
    else
    {
        QListIterator<QAction*> iter(_group->actions());
        while ( iter.hasNext() )
        {
            QAction* next = iter.next();
            if ( next->data() == key )
                _group->removeAction(next);
        }

        emit actionsChanged(_group->actions());
    }
}

void ProfileList::triggered(QAction* action)
{
    // assert that session key is still valid
    Q_ASSERT( SessionManager::instance()->sessionType( action->data().toString() ) );

    emit sessionSelected( action->data().toString() );
}

QList<QAction*> ProfileList::actions()
{
    return _group->actions();
}

#include "ProfileList.moc"
