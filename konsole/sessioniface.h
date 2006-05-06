#ifndef SESSIONIFACE_H
#define SESSIONIFACE_H

#include <dcopobject.h>
#include <QSize>

class SessionIface : virtual public DCOPObject
{
    K_DCOP
    k_dcop:

    virtual bool closeSession() =0;
    virtual bool sendSignal(int signal) =0;

    virtual void clearHistory() =0;
    virtual void renameSession(const QString &name) =0;
    virtual QString sessionName() =0;
    virtual int sessionPID() =0;
    
    virtual QString schema() =0;
    virtual void setSchema(const QString &schema) =0;
    virtual QString encoding() =0;
    virtual void setEncoding(const QString &encoding) =0;
    virtual QString keytab() =0;
    virtual void setKeytab(const QString &keyboard) =0;
    virtual QSize size() =0;
    virtual void setSize(QSize size) =0;
    virtual QString font() =0;
    virtual void setFont(const QString &font) =0;
};

#endif // SESSIONIFACE_H
