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
#include <kdialogbase.h>
#include <ksimpleconfig.h>
#include <kaction.h>
#include <kpopupmenu.h>

#include <kwinmodule.h>

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
#define VERSION "1.0.2"

class KRootPixmap;
class QCheckBox; 

// Defined in main.C
const char *konsole_shell(QStrList &args);

class Konsole : public KMainWindow
{
    Q_OBJECT

    friend class KonsoleSessionManaged;
public:

  Konsole(const char * name, const QString &_program, QStrList & _args,
    int histon, bool, const QString & _title, QCString type = 0, const QString &_term=QString::null, bool b_inRestore = false);
  ~Konsole();
  void setColLin(int columns, int lines);
  void setFullScreen(bool on);
  void initFullScreen();
  void initSessionSchema(int schemaNo);
  void initSessionFont(int fontNo);
  void initSessionTitle(const QString &_title);
  void initSessionKeyTab(const QString &keyTab);
  void newSession(const QString &program, const QStrList &args, const QString &term);

  void run();

public slots:

  void makeGUI();
  void newSession();

protected:

 bool queryClose();
 void saveProperties(KConfig* config);
 void readProperties(KConfig* config);
 void saveGlobalProperties(KConfig* config);
 void readGlobalProperties(KConfig* config);

 void showFullScreen();

private slots:
  void currentDesktopChanged(int desk);
  void slotBackgroundChanged(int desk);
  void configureRequest(TEWidget*,int,int,int);
  void activateSession();
  void activateSession(TESession*);
  void doneSession(TESession*,int);
  void opt_menu_activated(int item);
  void schema_menu_activated(int item);
  void pixmap_menu_activated(int item);
  void keytab_menu_activated(int item);
  void schema_menu_check();
  void tecRef();
  //what's this intended for ? aleXXX
  //void newSessionSelect();
  void newSession(int kind);
  void updateSchemaMenu();
  void updateKeytabMenu();

  void changeColumns(int);
  void notifySize(int,int);
  void updateTitle();
  void prevSession();
  void nextSession();
  void allowPrevNext();
  void setSchema(int n);
  void sendSignal(int n);
  void slotToggleToolbar();
  void slotToggleMenubar();
  void slotToggleFrame();
  void slotRenameSession();
  void slotHistoryType();
  void slotWordSeps();
  void slotSelectSize();
  void slotSelectFont();
  void slotSelectScrollbar();
  void slotWarnQuit();
  void loadScreenSessions();

private:
  KSimpleConfig *defaultSession();
  TESession *newSession(KSimpleConfig *co, QString pgm = QString::null, const QStrList &args = QStrList(), const QString &_term = QString::null);
  void readProperties(KConfig *config, const QString &schema);
  void applySettingsToGUI();
  void makeBasicGUI();
  void runSession(TESession* s);
  void addSession(TESession* s);
  void setColorPixmaps();

  void setSchema(const QString & path);
  void setSchema(ColorSchema* s);
  void setFont(int fontno);

  void addSessionCommand(const QString & path);
  void loadSessionCommands();
  void addScreenSession(const QString & socket);
  QSize calcSize(int columns, int lines);


  QPtrDict<TESession> action2session;
  QPtrDict<KRadioAction> session2action;
  QList<TESession> sessions;
  QIntDict<KSimpleConfig> no2command;

  TEWidget*      te;
  TESession*     se;
  TESession*     m_initialSession;
  ColorSchemaList* colors;

  KRootPixmap*   rootxpm;
  KWinModule*    kWinModule;

  KMenuBar*   menubar;
  KStatusBar* statusbar;

  KPopupMenu* m_file;
  KPopupMenu* m_sessions;
  KPopupMenu* m_options;
  KPopupMenu* m_schema;
  KPopupMenu* m_keytab;
//  KPopupMenu* m_codec;
  KPopupMenu* m_toolbarSessionsCommands;
  KPopupMenu* m_signals;
  KPopupMenu* m_help;

  KToggleAction *showToolbar;
  KToggleAction *showMenubar;
  KToggleAction *showScrollbar;
  KToggleAction *showFrame;
  KSelectAction *selectSize;
  KSelectAction *selectFont;
  KSelectAction *selectScrollbar;

  KToggleAction *warnQuit;                      // Warn when closing this session on quit

  int cmd_serial;
  int cmd_first_screen;
  int         n_keytab;
  int         n_defaultKeytab;
  int         n_font;
  int         n_defaultFont; // font as set in config to use as default for new sessions
  int         n_scroll;
  int         n_render;
  int         curr_schema; // current schema no
  int         wallpaperSource;
  QString     s_schema;
  QString     s_kconfigSchema;
  QString     s_word_seps;			// characters that are considered part of a word
  QString     pmPath; // pixmap path
  QString     dropText;
  QFont       defaultFont;
  QSize       defaultSize;

  QRect       _saveGeometry;

  bool        b_scroll:1;
  bool        b_framevis:1;
  bool        b_fullscreen:1;
  bool        m_menuCreated:1;
  bool        skip_exit_query:1;
  bool        b_warnQuit:1;
  bool        isRestored;
  bool        wasRestored;

  unsigned int m_histSize;
  bool         b_histEnabled:1;

public:

  QString     s_title;
};

class QSpinBox;

class HistoryTypeDialog : public KDialogBase 
{
    Q_OBJECT
public:
  HistoryTypeDialog(const HistoryType& histType,
                    unsigned int histSize,
                    QWidget *parent);

public slots:

  void slotHistEnable(bool);
  void slotDefault();

  unsigned int nbLines() const;
  bool isOn() const;

protected:
  QSpinBox*  m_size;
  QCheckBox* m_btnEnable;
};



#endif
