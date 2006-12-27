#ifndef SESSIONCONTROLLER_H
#define SESSIONCONTROLLER_H

// Qt
#include <QIcon>
#include <QList>
#include <QString>

// KDE
#include <kactioncollection.h>

class QAction;

class TESession;

/**
 *
 */
class SessionController : public QObject
{
Q_OBJECT
    
public:
    enum MonitorType
    {
        Silence,
        Activity
    };

    SessionController();

    /** Returns the current title of the session */
    QString title() const;
    /** Returns the icon currently associated with the session */
    QIcon icon() const;
   


    /** 
     * Attempt to rename the session, prompting the user for a new name 
     * Returns the new name of the session, or the previous name if the session name was not changed 
     */
    QString rename(); 
    /** 
     * Enable monitoring for certain types of events in the session 
     * @param type The type of event to monitor in the session 
     */
    void monitor(MonitorType type); 
    void sendToAllSessions();

signals:
    /** Emitted whenever the title of the session changes */
    void titleChanged( Controller* item );
    /** Emitted whenver the icon associated with the session changes */
    void iconChanged( Controller* item );

protected:
    /** Sets the title returned by title() */
    void setTitle( const QString& title );
    /** Sets the icon returned by icon() */
    void setIcon( const QIcon& icon );


    
private:
    QString _title;
    QIcon _icon;
};

#endif //SESSIONCONTROLLER_H
