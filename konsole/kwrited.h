#ifndef KWRITED_H
#define KWRITED_H

#include <qtextedit.h>
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
  QTextEdit* wid;
  KPty* pty;
};

class KWritedModule : public KDEDModule
{
  Q_OBJECT
  K_DCOP
public:
  KWritedModule( const QCString& obj );
 ~KWritedModule();
private:
  KWrited* pro;
};

#endif
