/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [main.h]                 Testbed for TE framework                          */
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

#ifndef MAIN_H
#define MAIN_H


#include <ktmainwindow.h>
//#include <ktmainwindow.h>
#include <ksimpleconfig.h>
#include <qstrlist.h>

#include "TEShell.h"
#include "TEWidget.h"
#include "TEmuVt102.h"
#include "session.h"
#include "schema.h"


class QDragEnterEvent;
class QDropEvent;

class KRootPixmap;

class TEDemo : public KTMainWindow
{ Q_OBJECT

public:

  TEDemo(const QString& name, QStrList & _args, int login_shell);
  ~TEDemo();
  void setColLin(int columns, int lines);

private slots:
  void configureRequest(TEWidget*,int,int,int);

  void scrollbar_menu_activated(int item);
  void activateSession(int);
  void doneSession(TESession*,int);
  void opt_menu_activated(int item);
  void font_menu_activated(int item);
  void schema_menu_activated(int item);
  void size_menu_activated(int item);
  void pixmap_menu_activated(int item);
  void drop_menu_activated(int item);
  void about();
  void help();
  void tecRef();
  void newSession(int kind);

  void changeColumns(int);
  void notifySize(int,int);
  void setHeader();
  void changeTitle(int, const char*s);
    
protected:

 void saveProperties(KConfig* config);
 void readProperties(KConfig* config);

 // Dnd
 void dragEnterEvent(QDragEnterEvent* event);
 void dropEvent(QDropEvent* event);

private slots:

  void setSchema(int n);

private:

  void makeMenu();
  void makeStatusbar();
  void runSession(TESession* s); 
  void addSession(TESession* s);
  void setColorPixmaps();

  void setMenuVisible(bool);
  void setFrameVisible(bool);
  void setBsHack(bool);
  
  void setSchema(const char* path);
  void setSchema(const ColorSchema* s);
  void setFont(int fontno);

  void addSessionCommand(const char* path);
  void loadSessionCommands();

private:

  TEWidget*      te;
  TESession*     se;

  KRootPixmap*   rootxpm;

  KMenuBar*   menubar;
  KStatusBar* statusbar;

  QPopupMenu* m_file;
  QPopupMenu* m_sessions;
  QPopupMenu* m_options;
  QPopupMenu* m_scrollbar;
  QPopupMenu* m_font;
  QPopupMenu* m_schema;
  QPopupMenu* m_size;
  QPopupMenu* m_drop;
//
  bool        b_menuvis;
  bool        b_framevis;
  bool        b_bshack;
  int         n_font;
  int         n_scroll;
  QString     s_schema;
  int         n_render;
  QString     pmPath; // pixmap path
  QString     dropText;
  QSize       defaultSize;
  int         curr_schema; // current schema no
  QFont       defaultFont;

  QStrList args;

public:

  QString     title;
};

#endif
