#ifndef SESSIONIFACE_H
#define SESSIONIFACE_H

#include <dcopobject.h>

class SessionIface : virtual public DCOPObject
{
    K_DCOP
    k_dcop:

    virtual bool closeSession() =0;
    virtual bool sendSignal(int signal) =0;

    virtual void clearHistory() =0;
    virtual void renameSession(const QString &name) =0;
    virtual QString sessionName() =0;
};

#endif // SESSIONIFACE_H
