#ifndef KONSOLEIFACE_H
#define KONSOLEIFACE_H

#include <dcopobject.h>

class KonsoleIface : virtual public DCOPObject
{
    K_DCOP
    k_dcop:

    virtual void newSession() = 0;
    virtual void newSession(const QString &type) = 0;

    virtual int currentSession() = 0;
    virtual int sessionCount() = 0;
    virtual void setCurrentSession(const int session) = 0;
    virtual void closeCurrentSession() = 0;
    virtual void renameCurrentSession(const QString &name) = 0;
    virtual void feedAllSessions(const QString &text) = 0;
    virtual void feedCurrentSession(const QString &text) = 0;
    virtual void sendAllSessions(const QString &text) = 0;
    virtual void sendCurrentSession(const QString &text) = 0;
    virtual void sendCurrentSessionSignal(const int signal) = 0;

    virtual void nextSession() = 0;
    virtual void prevSession() = 0;
    virtual void moveSessionLeft() = 0;
    virtual void moveSessionRight() = 0;
    virtual void setFullScreen(bool on) =0;
};

#endif // KONSOLEIFACE_H
