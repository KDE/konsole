#ifndef KWRITED_H
#define KWRITED_H

#include <qtextedit.h>
#include <kdedmodule.h>
#include <qpopupmenu.h>
#include <qtextedit.h>

class KPty;

class KWrited : public QTextEdit
{ Q_OBJECT
public:
  KWrited();
 ~KWrited();
protected:
  virtual QPopupMenu *createPopupMenu(const QPoint &);
private slots:
  void block_in(int fd);
  void clearText(void);
private:
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
