/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [session.h]              Testbed for TE framework                          */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole, an X terminal.                               */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#ifndef SESSION_H
#define SESSION_H

#include <kapp.h>
#include <ktmainwindow.h>
#include <qstrlist.h>

#include "TEPty.h"
#include "TEWidget.h"
#include "TEmuVt102.h"

class TESession : public QObject
{ Q_OBJECT

public:

  TESession(KTMainWindow* main, TEWidget* w,
            const char* pgm, QStrList & _args,
	    const char* term);
  ~TESession();

public:

  void        setConnect(bool r);
  TEmulation* getEmulation();      // to control emulation
  bool        isSecure();

public:

  int schemaNo();
  int fontNo();
  const char* emuName();
  const QString& Title();
  bool history();
  int keymap();

  void setHistory(bool on);
  void setSchemaNo(int sn);
  void setKeymapNo(int kn);
  void setFontNo(int fn);
  void setTitle(const QString& title);
  void kill(int signal);

public slots:

  void run();
  void done(int status);
  void terminate();

signals:

  void done(TESession*, int);

private:

  TEPty*         sh;
  TEWidget*      te;
  TEmulation*    em;

  //FIXME: using the indices here
  // is propably very bad. We should
  // use a persistent reference instead.
  int            schema_no;
  int            font_no;
  int            keymap_no;
  QString        title;

  const char*    pgm;
  QStrList       args;

  QCString       term;
};

#endif
