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


#include <ktmainwindow.h>
#include <ksimpleconfig.h>
#include <kaction.h>
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
#define VERSION "0.9.15"

class QDragEnterEvent;
class QDropEvent;

class KAccel;
class KRootPixmap;

class Konsole : public KTMainWindow
{ Q_OBJECT

public:

  Konsole(const char * name, const char* pgm, QStrList & _args, int histon);
  ~Konsole();
  void setColLin(int columns, int lines);
  void setFullScreen(bool on);

private slots:
  void configureRequest(TEWidget*,int,int,int);

//  void scrollbar_menu_activated(int item);
  void activateSession();

  void doneSession(TESession*,int);
  void opt_menu_activated(int item);
  void schema_menu_activated(int item);
  void pixmap_menu_activated(int item);
  void keytab_menu_activated(int item);
  void tecRef();
  void newSession();
  void newSession(int kind);
  void newSessionSelect();

  void changeColumns(int);
  void notifySize(int,int);
  void setHeader();
  void changeTitle(int, const QString&);

protected:

 void saveProperties(KConfig* config);
 void readProperties(KConfig* config);
 void saveGlobalProperties(KConfig* config);
 void readGlobalProperties(KConfig* config);

 // Dnd
 //void dragEnterEvent(QDragEnterEvent* event);

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

  //  void setFrameVisible(bool);
  void setHistory(bool);

  void setSchema(const QString & path);
  void setSchema(const ColorSchema* s);
  void setFont(int fontno);

  void addSessionCommand(const QString & path);
  void loadSessionCommands();

private:

  //////// Those used to be as static in konsole.C, leading
  /////// to crashes on Solaris (non initialised)
  ////// I suppose it's ok to make them class specific
  ///// (after all, Konsole == window, and this could be different for
  //// two windows....). Make them static pointers initialised in the
  /// constructor if this is not ok.
  // (David)
  int session_no;
  QPtrDict<TESession> action2session;
  QPtrDict<KRadioAction> session2action;
  QIntDict<KSimpleConfig> no2command;
  int cmd_serial;

  TEWidget*      te;
  TESession*     se;

  KRootPixmap*   rootxpm;

  KMenuBar*   menubar;
  KStatusBar* statusbar;
  KAccel*     accel;

  QPopupMenu* m_file;
  QPopupMenu* m_sessions;
  QPopupMenu* m_options;
  //  QPopupMenu* m_scrollbar;
  //  QPopupMenu* m_font;
  QPopupMenu* m_schema;
  QPopupMenu* m_keytab;
  QPopupMenu* m_codec;
  //  QPopupMenu* m_size;
  QPopupMenu* m_drop;
//
  KToggleAction *showToolbar;
  bool        b_toolbarvis;
  KToggleAction *showMenubar;
  bool        b_menuvis;
  KToggleAction *showScrollbar;
  bool        b_scroll;
  KToggleAction *showFrame;
  bool        b_framevis;
  KSelectAction *selectSize;
  KSelectAction *selectFont;
  KSelectAction *selectScrollbar;

  int         n_toolbarpos;
  int         n_keytab;
  int         n_font;
  int         n_scroll;
  QString     s_schema;
  int         n_render;
  QString     pmPath; // pixmap path
  QString     dropText;
  QSize       defaultSize;
  int         curr_schema; // current schema no
  QFont       defaultFont;

  const char* pgm;
  QStrList    args;

  bool        b_fullscreen;
  QRect       _saveGeometry;

public:

  QString     title;
};

#endif
