/* ----------------------------------------------------------------------- */
/*                                                                         */
/* [konsole.h]                      Konsole                                   */
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
/* -------------------------------------------------------------------------- */

#ifndef KONSOLE_H
#define KONSOLE_H


#include <kmainwindow.h>
#include <ksimpleconfig.h>
#include <kaction.h>
#include <kpopupmenu.h>
#include <qstrlist.h>
#include <qintdict.h>
#include <qptrdict.h>

#include "TEPty.h"
#include "TEWidget.h"
#include "TEmuVt102.h"
#include "session.h"
#include "schema.h"

#undef PACKAGE
#undef VERSION
#define PACKAGE "konsole"
#define VERSION "1.0.1"

class KRootPixmap;

class Konsole : public KMainWindow
{ Q_OBJECT

public:

  Konsole(const char * name, const char* pgm, QStrList & _args, int histon, bool);
  ~Konsole();
  void setColLin(int columns, int lines);
  void setFullScreen(bool on);

private slots:
  void configureRequest(TEWidget*,int,int,int);
  void activateSession();
  void activateSession(TESession*);
  void doneSession(TESession*,int);
  void opt_menu_activated(int item);
  void schema_menu_activated(int item);
  void pixmap_menu_activated(int item);
  void keytab_menu_activated(int item);
  void tecRef();
  void newSession();
  void newSessionSelect();
  void newSession(int kind);

  void changeColumns(int);
  void notifySize(int,int);
  void setHeader();
  void changeTitle(int, const QString&);
  void prevSession();
  void nextSession();
  void allowPrevNext();

protected:

 void saveProperties(KConfig* config);
 void readProperties(KConfig* config);
 void saveGlobalProperties(KConfig* config);
 void readGlobalProperties(KConfig* config);

 void showFullScreen();

private slots:

  void setSchema(int n);
  void sendSignal(int n);
  void slotToggleToolbar();
  void slotToggleMenubar();
  void slotToggleFrame();
  void slotRenameSession();
  void slotSelectSize();
  void slotSelectFont();
  void slotSelectScrollbar();

private:

  void makeMenu();
  void runSession(TESession* s);
  void addSession(TESession* s);
  void setColorPixmaps();

  void setHistory(bool);

  void setSchema(const QString & path);
  void setSchema(const ColorSchema* s);
  void setFont(int fontno);

  void addSessionCommand(const QString & path);
  void loadSessionCommands();
  QSize calcSize(int columns, int lines);

private:

  int session_no;
  QPtrDict<TESession> action2session;
  QPtrDict<KRadioAction> session2action;
  QList<TESession> sessions;
  QIntDict<KSimpleConfig> no2command;
  int cmd_serial;

  TEWidget*      te;
  TESession*     se;

  KRootPixmap*   rootxpm;

  KMenuBar*   menubar;
  KStatusBar* statusbar;

  KPopupMenu* m_file;
  KPopupMenu* m_sessions;
  KPopupMenu* m_options;
  KPopupMenu* m_schema;
  KPopupMenu* m_keytab;
  KPopupMenu* m_codec;
  KPopupMenu* m_toolbarSessionsCommands;

  KToggleAction *showToolbar;
  KToggleAction *showMenubar;
  KToggleAction *showScrollbar;
  bool        b_scroll;
  KToggleAction *showFrame;
  bool        b_framevis;
  KSelectAction *selectSize;
  KSelectAction *selectFont;
  KSelectAction *selectScrollbar;

  int         n_keytab;
  int         n_font;
  int         n_scroll;
  QString     s_schema;
  int         n_render;
  QString     pmPath; // pixmap path
  QString     dropText;
  int         curr_schema; // current schema no
  QFont       defaultFont;
  QSize       defaultSize;

  const char* pgm;
  QStrList    args;

  bool        b_fullscreen;
  QRect       _saveGeometry;

public:

  QString     title;
};

#endif
