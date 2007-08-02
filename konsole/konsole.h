/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1996 by Matthias Ettrich <ettrich@kde.org>
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/
/* The material contained in here more or less directly orginates from    */
/* kvt, which is copyright (c) 1996 by Matthias Ettrich <ettrich@kde.org> */
/*                                                                        */

#ifndef KONSOLE_H
#define KONSOLE_H


#include <kmainwindow.h>
#include <kdialogbase.h>
#include <ksimpleconfig.h>
#include <keditcl.h>

#include <kwinmodule.h>

#include <qstrlist.h>
#include <qintdict.h>
#include <qptrdict.h>
#include <qsignalmapper.h>

#include "TEPty.h"
#include "TEWidget.h"
#include "TEmuVt102.h"
#include "session.h"
#include "schema.h"
#include "konsolebookmarkmenu.h"
#include "konsolebookmarkhandler.h"

#include "konsoleiface.h"

#define KONSOLE_VERSION "1.6.6"

class KRootPixmap;
class QLabel;
class QCheckBox;
class KonsoleFind;
class KPopupMenu;
class KAction;
class KToggleAction;
class KSelectAction;
class KRadioAction;
class KTabWidget;
class QToolButton;

// Defined in main.C
const char *konsole_shell(QStrList &args);

class Konsole : public KMainWindow, virtual public KonsoleIface
{
    Q_OBJECT

    friend class KonsoleSessionManaged;
public:

  Konsole(const char * name, int histon, bool menubaron, bool tabbaron,
    bool frameon, bool scrollbaron,
    QCString type = 0, bool b_inRestore = false, const int wanted_tabbar = 0,
    const QString &workdir=QString::null);

  ~Konsole();
  void setColLin(int columns, int lines);
  void setAutoClose(bool on);
  void initFullScreen();
  void initSessionFont(int fontNo);
  void initSessionFont(QFont f);
  void initSessionKeyTab(const QString &keyTab);
  void initMonitorActivity(bool on);
  void initMonitorSilence(bool on);
  void initMasterMode(bool on);
  void initTabColor(QColor color);
  void initHistory(int lines, bool enable);
  void newSession(const QString &program, const QStrList &args, const QString &term, const QString &icon, const QString &title, const QString &cwd);
  void setSchema(const QString & path);
  void setEncoding(int);
  void setSessionTitle(QString&, TESession* = 0);
  void setSessionEncoding(const QString&, TESession* = 0);

  void enableFullScripting(bool b);
  void enableFixedSize(bool b);

  void setDefaultSession(const QString &filename);
  void showTipOnStart();

  // Additional functions for DCOP
  int sessionCount() { return sessions.count(); }

  QString currentSession();
  QString newSession(const QString &type);
  QString sessionId(const int position);

  void activateSession(const QString& sessionId);
  void feedAllSessions(const QString &text);
  void sendAllSessions(const QString &text);

  KURL baseURL() const;

  virtual bool processDynamic(const QCString &fun, const QByteArray &data, QCString& replyType, QByteArray &replyData);
  virtual QCStringList functionsDynamic();

  void callReadPropertiesInternal(KConfig *config, int number) { readPropertiesInternal(config,number); }

  enum TabPosition { TabNone, TabTop, TabBottom };
  enum TabViewModes { ShowIconAndText = 0, ShowTextOnly = 1, ShowIconOnly = 2 };

public slots:
  void activateSession(int position);
  void activateSession(QWidget*);
  void slotUpdateSessionConfig(TESession *session);
  void slotResizeSession(TESession*, QSize);
  void slotSetSessionEncoding(TESession *session, const QString &encoding);
  void slotGetSessionSchema(TESession *session, QString &schema);
  void slotSetSessionSchema(TESession *session, const QString &schema);

  void makeGUI();
  QString newSession();

protected:

 bool queryClose();
 void saveProperties(KConfig* config);
 void readProperties(KConfig* config);

private slots:
  void configureRequest(TEWidget*,int,int,int);
  void activateSession();
  void activateSession(TESession*);
  void closeCurrentSession();
  void confirmCloseCurrentSession(TESession* _se=0);
  void doneSession(TESession*);
  void slotCouldNotClose();
  void toggleFullScreen();
  bool fullScreen();
  void setFullScreen(bool on);
  void schema_menu_activated(int item);
  void pixmap_menu_activated(int item, TEWidget* tewidget=0);
  void keytab_menu_activated(int item);
  void schema_menu_check();
  void attachSession(TESession*);
  void slotDetachSession();
  void bookmarks_menu_check();
  void newSession(int kind);
  void newSessionTabbar(int kind);
  void updateSchemaMenu();
  void updateKeytabMenu();
  void updateRMBMenu();

  void changeTabTextColor(TESession*, int);
  void changeColumns(int);
  void changeColLin(int columns, int lines);
  void notifySessionState(TESession* session,int state);
  void notifySize(int columns, int lines);
  void updateTitle(TESession* _se=0);
  void prevSession();
  void nextSession();
  void activateMenu();
  void slotMovedTab(int,int);
  void moveSessionLeft();
  void moveSessionRight();
  void allowPrevNext();
  void setSchema(int n, TEWidget* tewidget=0);   // no slot necessary?
  void sendSignal(int n);
  void slotClearTerminal();
  void slotResetClearTerminal();
  void slotSelectTabbar();
  void slotToggleMenubar();
  void slotRenameSession();
  void slotRenameSession(TESession* ses, const QString &name);
  void slotToggleMonitor();
  void slotToggleMasterMode();
  void slotClearAllSessionHistories();
  void slotHistoryType();
  void slotClearHistory();
  void slotFindHistory();
  void slotSaveHistory();
  void slotSelectBell();
  void slotSelectSize();
  void slotSelectFont();
  void slotInstallBitmapFonts();
  void slotSelectScrollbar();
  void loadScreenSessions();
  void updateFullScreen(bool on);

  void slotSaveSettings();
  void slotSaveSessionsProfile();
  void slotConfigureNotifications();
  void slotConfigureKeys();
  void slotConfigure();
  void reparseConfiguration();

  void disableMasterModeConnections();
  void enableMasterModeConnections();
  void enterURL( const QString&, const QString& );
  void newSession( const QString&, const QString& );

  void slotFind();
  void slotFindDone();
  void slotFindNext();
  void slotFindPrevious();

  void showTip();

  void slotSetSelectionEnd() { te->setSelectionEnd(); }
  void slotCopyClipboard() { te->copyClipboard(); }
  void slotPasteClipboard() { te->pasteClipboard(); }
  void slotPasteSelection() { te->pasteSelection(); }

  void listSessions();
  void switchToSession();

  void biggerFont();
  void smallerFont();

  void slotZModemDetected(TESession *session);
  void slotZModemUpload();

  void slotPrint();

  void toggleBidi();

  void slotTabContextMenu(QWidget*, const QPoint &);
  void slotTabDetachSession();
  void slotTabRenameSession();
  void slotTabSelectColor();
  void slotTabCloseSession();
  void slotTabToggleMonitor();
  void slotTabToggleMasterMode();
  void slotTabbarContextMenu(const QPoint &);
  void slotTabSetViewOptions(int);
  void slotTabbarToggleDynamicHide();
  void slotToggleAutoResizeTabs();
  void slotFontChanged();

  void slotSetEncoding();
private:
  KSimpleConfig *defaultSession();
  QString newSession(KSimpleConfig *co, QString pgm = QString::null, const QStrList &args = QStrList(),
                     const QString &_term = QString::null, const QString &_icon = QString::null,
                     const QString &_title = QString::null, const QString &_cwd = QString::null);
  void readProperties(KConfig *config, const QString &schema, bool globalConfigOnly);
  void applySettingsToGUI();
  void makeTabWidget();
  void makeBasicGUI();
  void runSession(TESession* s);
  void addSession(TESession* s);
  void detachSession(TESession* _se=0);
  void setColorPixmaps();
  void renameSession(TESession* ses);

  void setSchema(ColorSchema* s, TEWidget* tewidget=0);
  void setMasterMode(bool _state, TESession* _se=0);

  void buildSessionMenus();
  void addSessionCommand(const QString & path);
  void loadSessionCommands();
  void createSessionMenus();
  void addScreenSession(const QString & path, const QString & socket);
  void resetScreenSessions();
  void checkBitmapFonts();

  void initTEWidget(TEWidget* new_te, TEWidget* default_te);

  void createSessionTab(TEWidget *widget, const QIconSet& iconSet,
                        const QString &text, int index = -1);
  QIconSet iconSetForSession(TESession *session) const;

  bool eventFilter( QObject *o, QEvent *e );

  QPtrList<TEWidget> activeTEs();

  QPtrDict<TESession> action2session;
  QPtrDict<KRadioAction> session2action;
  QPtrList<TESession> sessions;

  QIntDict<KSimpleConfig> no2command;     //QT4 - convert to QList

  KSimpleConfig* m_defaultSession;
  QString m_defaultSessionFilename;

  KTabWidget* tabwidget;
  TEWidget*      te;     // the visible TEWidget, either sole one or one of many
  TESession*     se;
  TESession*     se_previous;
  TESession*     m_initialSession;
  ColorSchemaList* colors;
  QString        s_encodingName;

  QPtrDict<KRootPixmap> rootxpms;
  KWinModule*    kWinModule;

  KMenuBar*   menubar;
  KStatusBar* statusbar;

  KPopupMenu* m_session;
  KPopupMenu* m_edit;
  KPopupMenu* m_view;
  KPopupMenu* m_bookmarks;
  KPopupMenu* m_bookmarksSession;
  KPopupMenu* m_options;
  KPopupMenu* m_schema;
  KPopupMenu* m_keytab;
  KPopupMenu* m_tabbarSessionsCommands;
  KPopupMenu* m_signals;
  KPopupMenu* m_help;
  KPopupMenu* m_rightButton;
  KPopupMenu* m_sessionList;
  KPopupMenu* m_tabPopupMenu;
  KPopupMenu* m_tabPopupTabsMenu;
  KPopupMenu* m_tabbarPopupMenu;

  KAction *m_zmodemUpload;
  KToggleAction *monitorActivity, *m_tabMonitorActivity;
  KToggleAction *monitorSilence, *m_tabMonitorSilence;
  KToggleAction *masterMode, *m_tabMasterMode;
  KToggleAction *showMenubar;
  KToggleAction *m_fullscreen;

  KSelectAction *selectSize;
  KSelectAction *selectFont;
  KSelectAction *selectScrollbar;
  KSelectAction *selectTabbar;
  KSelectAction *selectBell;
  KSelectAction *selectSetEncoding;

  KAction       *m_clearHistory;
  KAction       *m_findHistory;
  KAction       *m_findNext;
  KAction       *m_findPrevious;
  KAction       *m_saveHistory;
  KAction       *m_detachSession;
  KAction       *m_moveSessionLeft;
  KAction       *m_moveSessionRight;

  KAction       *m_copyClipboard;
  KAction       *m_pasteClipboard;
  KAction       *m_pasteSelection;
  KAction       *m_clearTerminal;
  KAction       *m_resetClearTerminal;
  KAction       *m_clearAllSessionHistories;
  KAction       *m_renameSession;
  KAction       *m_saveProfile;
  KAction       *m_closeSession;
  KAction       *m_print;
  KAction       *m_quit;
  KAction       *m_tabDetachSession;

  KActionCollection *m_shortcuts;

  KonsoleBookmarkHandler *bookmarkHandler;
  KonsoleBookmarkHandler *bookmarkHandlerSession;

  KonsoleFind* m_finddialog;
  bool         m_find_first;
  bool         m_find_found;
  QString      m_find_pattern;

  int cmd_serial;
  int cmd_first_screen;
  int         n_keytab;
  int         n_defaultKeytab;
  int         n_scroll;
  int         n_tabbar;
  int         n_bell;
  int         n_render;
  int         curr_schema; // current schema no
  int         wallpaperSource;
  int         sessionIdCounter;
  int         monitorSilenceSeconds;

  QString     s_schema;
  QString     s_kconfigSchema;
  QString     s_word_seps;			// characters that are considered part of a word
  QString     pmPath; // pixmap path
  QString     dropText;
  QFont       defaultFont;
  QSize       defaultSize;

  QRect       _saveGeometry;

  QTimer      m_closeTimeout;

  TabViewModes m_tabViewMode;
  bool        b_dynamicTabHide;
  bool        b_autoResizeTabs;
  bool        b_installBitmapFonts;

  bool        b_framevis:1;
  bool        b_fullscreen:1;
  bool        m_menuCreated:1;
  bool        b_warnQuit:1;
  bool        isRestored:1;
  bool        b_allowResize:1; // Whether application may resize
  bool        b_fixedSize:1; // Whether user may resize
  bool        b_addToUtmp:1;
  bool        b_xonXoff:1;
  bool        b_bidiEnabled:1;

  bool        b_histEnabled:1;
  bool        b_fullScripting:1;
  bool        b_showstartuptip:1;
  bool        b_sessionShortcutsEnabled:1;
  bool        b_sessionShortcutsMapped:1;
  bool        b_matchTabWinTitle:1;

  unsigned int m_histSize;
  int m_separator_id;

  TESession*  m_contextMenuSession;

  QToolButton* m_newSessionButton;
  QToolButton* m_removeSessionButton;
  QPoint      m_newSessionButtonMousePressPos;

  QSignalMapper* sessionNumberMapper;
  QStringList    sl_sessionShortCuts;
  QString  s_workDir;

  QColor    m_tabColor;
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
  void slotSetUnlimited();

  unsigned int nbLines() const;
  bool isOn() const;

protected:
  QLabel*        m_label;
  QSpinBox*      m_size;
  QCheckBox*     m_btnEnable;
  QPushButton*   m_setUnlimited;
};

class SizeDialog : public KDialogBase
{
    Q_OBJECT
public:
  SizeDialog(unsigned int const columns,
             unsigned int const lines,
             QWidget *parent);

public slots:
  void slotDefault();

  unsigned int columns() const;
  unsigned int lines() const;

protected:
  QSpinBox*  m_columns;
  QSpinBox*  m_lines;
};

class KonsoleFind : public KEdFind
{
    Q_OBJECT
public:
  KonsoleFind( QWidget *parent = 0, const char *name=0, bool modal=true );
  bool reg_exp() const;

private slots:
  void slotEditRegExp();

private:
  QCheckBox*    m_asRegExp;
  QDialog*      m_editorDialog;
  QPushButton*  m_editRegExp;
};

#endif
