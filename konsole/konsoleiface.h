#ifndef KONSOLEIFACE_H
#define KONSOLEIFACE_H

#include <dcopobject.h>

class KonsoleIface : virtual public DCOPObject
{
    K_DCOP
    k_dcop:

    virtual int sessionCount() = 0;

    virtual QString currentSession() = 0;
    virtual QString newSession() = 0;
    virtual QString newSession(const QString &type) = 0;
    virtual QString sessionId(const int position) = 0;

    virtual void activateSession(const QString &sessionId) = 0;

    virtual void nextSession() = 0;
    virtual void prevSession() = 0;
    virtual void moveSessionLeft() = 0;
    virtual void moveSessionRight() = 0;
    virtual void setFullScreen(bool on) = 0;
    virtual ASYNC reparseConfiguration() = 0;
};

#endif // KONSOLEIFACE_H
