#ifndef KWRITED_H
#define KWRITED_H

#include <q3textedit.h>
#include <kdedmodule.h>
#include <QTextEdit>
#include <QMenu>

class KPty;

class KWrited : public QTextEdit
{ Q_OBJECT
public:
  KWrited();
 ~KWrited();
protected:
  virtual void contextMenuEvent(QContextMenuEvent *);

private Q_SLOTS:
  void block_in(int fd);
  void clearText(void);
private:
  KPty* pty;
};

class KWritedModule : public KDEDModule
{
  Q_OBJECT
public:
  KWritedModule();
 ~KWritedModule();
private:
  KWrited* pro;
};

#endif
