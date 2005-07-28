#ifndef KWRITED_H
#define KWRITED_H

#include <q3textedit.h>
//Added by qt3to4:
#include <Q3CString>
#include <kdedmodule.h>

class KPty;

class KWrited : public QObject
{ Q_OBJECT
public:
  KWrited();
 ~KWrited();
private slots:
  void block_in(int fd);
private:
  Q3TextEdit* wid;
  KPty* pty;
};

class KWritedModule : public KDEDModule
{
  Q_OBJECT
  K_DCOP
public:
  KWritedModule( const Q3CString& obj );
 ~KWritedModule();
private:
  KWrited* pro;
};

#endif
