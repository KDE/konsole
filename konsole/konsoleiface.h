#ifndef KONSOLEIFACE_H
#define KONSOLEIFACE_H

#include <dcopobject.h>

class KonsoleIface : virtual public DCOPObject
{
    K_DCOP
    k_dcop:

    virtual int sessionCount() = 0;

    virtual int currentSession() = 0;
    virtual void setCurrentSession( int ) = 0;

    virtual void nextSession() = 0;
    virtual void prevSession() = 0;

    virtual void newSession() = 0;
};

#endif // KONSOLEIFACE_H
