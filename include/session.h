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
/* The material contained in here more or less directly orginates from        */
/* kvt, which is copyright (c) 1996 by Matthias Ettrich <ettrich@kde.org>     */
/*                                                                            */
/* The whole program is copyleft under the GNU General Public Licence.        */
/* See COPYING, the documenation, or <http://www.gnu.org> for details.        */
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

  TESession(KTMainWindow* main, TEWidget* w, char* args[], const char* term);
  ~TESession();

public:

  void       setConnect(bool r);
  Emulation* getEmulation();      // to control emulation

public:

  int schemaNo();
  int fontNo();
  const char* term();

  void setSchemaNo(int sn);
  void setFontNo(int fn);

public slots:

  void done();

signals:

  void done(TESession*);

private:

  Shell*         sh;
  TEWidget*      te;
  Emulation*     em;

  int            schema_no; // no exactly the right place
  int            font_no;   // no exactly the right place
  QString        emuname;
};

#endif
