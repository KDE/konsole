// Qt
#include <QAction>
#include <QActionGroup>

// KDE
#include <KIcon>

// Konsole
#include "SessionList.h"
#include "SessionManager.h"

using namespace Konsole;

SessionList::SessionList(SessionManager* manager , QObject* parent)
    : QObject(parent)
    , _manager(manager)
{
    // construct the list of session types

    _group = new QActionGroup(this);

    QListIterator<QString> iter(manager->availableSessionTypes());

    while (iter.hasNext())
    {
        const QString& key = iter.next();
        SessionInfo* info = manager->sessionType(key);

        QAction* action = new QAction(_group);
        action->setText( info->name() );
        action->setIcon( KIcon(info->icon()) );
        action->setData( key );
    }

    connect( _group , SIGNAL(triggered(QAction*)) , this , SLOT(triggered(QAction*)) );
}

void SessionList::triggered(QAction* action)
{
    emit sessionSelected( action->data().toString() );
}

QList<QAction*> SessionList::actions()
{
    return _group->actions();
}

#include "SessionList.moc"
