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

#include "TEShell.h"
#include "TEWidget.h"
#include "TEmuVt102.h"

class TESession : QObject
{ Q_OBJECT

public:

  TESession(KTMainWindow* main, TEWidget* w, const char* args[], const char* term);
  ~TESession();

public:

  void       setConnect(bool r);
  Emulation* getEmulation();      // to control emulation

public:

  int schemaNo();
  int fontNo();
  const char* emuName();
  const char* Title();

  void setSchemaNo(int sn);
  void setFontNo(int fn);
  void setTitle(const char* title);

  void run();

public slots:

  void done(int status);

signals:

  void done(TESession*, int);

private:

  Shell*         sh;
  TEWidget*      te;
  Emulation*     em;

  int            schema_no; // no exactly the right place
  int            font_no;   // no exactly the right place

  char*          term;
  char**         args;
  QString        title;
};

#endif
