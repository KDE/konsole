/*
    This file is part of Konsole, an X terminal.
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

#ifndef SESSION_H
#define SESSION_H

// Qt
#include <QStringList>
#include <QByteArray>

// KDE
#include <kapplication.h>
#include <kmainwindow.h>

// Konsole
#include "TEPty.h"
#include "TEWidget.h"
#include "TEmuVt102.h"


class KProcIO;
class KProcess;
class ZModemDialog;

class ColorSchema;

/**
 * TESession represents a Konsole session.
 * This consists of a pseudo-teletype (or PTY) which handles I/O between the terminal
 * process and Konsole, and a terminal emulation ( TEmulation and subclasses ) which
 * processes the output stream from the PTY and produces a character image which
 * is then shown on displays which are connected to the session.
 *
 * Each TESession can be connected to one or more views by using the addView() method.
 * The attached views can then display output from the program running in the terminal
 * or send input to the program in the terminal in the form of keypresses and mouse
 * activity.
 */
class TESession : public QObject
{ Q_OBJECT

public:
  Q_PROPERTY(QString sessionName READ sessionName)
  Q_PROPERTY(QString encoding READ encoding WRITE setEncoding)
  Q_PROPERTY(int sessionPid READ sessionPid)
  Q_PROPERTY(QString font READ font WRITE setFont)
  Q_PROPERTY(QString keytab READ keytab WRITE setKeytab)
  Q_PROPERTY(ColorSchema* schema READ schema WRITE setSchema)
  Q_PROPERTY(QSize size READ size WRITE setSize)

  TESession();
  ~TESession();

  /** 
   * Adds a new view for this session.    
   * 
   * The viewing widget will display the output from the terminal and input from the viewing widget 
   * (key presses, mouse activity etc.) will be sent to the terminal.
   *
   * Since terminal applications assume a single terminal screen, all views of a session
   * will display the same number of lines and columns.
   *
   * When the TESession instance is destroyed, any views which are still attached will also
   * be deleted.
   */
  void addView(TEWidget* widget);
  /** 
   * Removes a view from this session.  
   *
   * @p widget will no longer display output from or send input
   * to the terminal 
   */
  void removeView(TEWidget* widget);

  /**
   * Returns the views connected to this session
   */
  QList<TEWidget*> views() const;

  /**
   * Returns the primary view for this session.
   *
   * The primary view is the first view added to the session, which is used by the emulation
   * if it needs to determine the size of the view - ie. the number of lines and columns which
   * the view can display. 
   *
   * If the primary view is removed from the session using removeView(), the next view which is still
   * attached will become the primary view.
   *
   * TODO:  Remove this method and ensure that TESession works even if there are no views
   * attached.
   */
  TEWidget* primaryView();

  /** 
   * Returns true if the session has created child processes which have not yet terminated 
   * This call may be expensive if there are a large number of processes running. 
   */
  bool        hasChildren();

  void        setConnect(bool r);  // calls setListenToKeyPress(r)
  void        setListenToKeyPress(bool l);
  TEmulation* getEmulation();      // to control emulation
  bool        isSecure();
  bool        isMonitorActivity();
  bool        isMonitorSilence();
  bool        isMasterMode();
  int schemaNo();
  int encodingNo();
  int fontNo();

  /** 
   * Returns the value of the TERM environment variable which will be used in the session's
   * environment when it is started using the run() method.
   * Defaults to "xterm".
   */  
  const QString& terminalType() const;
  /** 
   * Sets the value of the TERM variable which will be used in the session's environment
   * when it is started using the run() method.  Changing this once the session has been
   * started using run() has no effect
   * Defaults to "xterm" if not set explicitly
   */
  void setTerminalType(const QString& terminalType);

  int sessionId() const;
  const QString& title() const;
  const QString& iconName() const;
  const QString& iconText() const;

  /** 
   * Return the session title set by the user (ie. the program running in the terminal), or an
   * empty string if the user has not set a custom title
   */
  QString userTitle() const;
  /** 
   * Returns the title of the session for display in UI widgets (eg. window captions)
   */
  QString displayTitle() const;

  int keymapNo();
  QString keymap();
  QStringList getArgs();
  QString getPgm();

  /** 
   * Sets the command line arguments which the session's program will be passed when
   * run() is called.
   */
  void setArguments(const QStringList& arguments);
  /** Sets the program to be executed when run() is called. */
  void setProgram(const QString& program);

  /** Returns the session's current working directory. */
  QString currentWorkingDirectory();
  QString initialWorkingDirectory() { return initial_cwd; }
  
  /** 
   * Sets the initial working directory for the session when it is run 
   * This has no effect once the session has been started.
   */
  void setInitialWorkingDirectory( const QString& dir ) { initial_cwd = dir; }
 
  void setHistory(const HistoryType&);
  const HistoryType& history();

  void setMonitorActivity(bool);
  void setMonitorSilence(bool);
  void setMonitorSilenceSeconds(int seconds);
  void setMasterMode(bool);
  
    //TODO - Remove these functions which use indicies to reference keyboard layouts,
    //       encodings etc. and replace them either with methods that uses pointers or references
    //       to the font object / keyboard layout object etc. or a QString key
    void setEncodingNo(int index);
    void setKeymapNo(int kn);
    void setFontNo(int fn);
  
  void setKeymap(const QString& _id);
  void setTitle(const QString& _title);
  void setIconName(const QString& _iconName);
  void setIconText(const QString& _iconText);
  void setAddToUtmp(bool);
  void setXonXoff(bool);
  bool testAndSetStateIconName (const QString& newname);
  bool sendSignal(int signal);

  void setAutoClose(bool b) { autoClose = b; }
  void renameSession(const QString &name);

  bool closeSession();
  void clearHistory();
  void feedSession(const QString &text);
  void sendSession(const QString &text);
  QString sessionName() { return _title; }
  int sessionPid() { return _shellProcess->pid(); }
  void enableFullScripting(bool b);

  void startZModem(const QString &rz, const QString &dir, const QStringList &list);
  void cancelZModem();
  bool zmodemIsBusy() { return zmodemBusy; }

  void print(QPainter &paint, bool friendly, bool exact);

 // QString schema();
 // void setSchema(const QString &schema);
  
  ColorSchema* schema();
  void setSchema(ColorSchema* schema);
  
  QString encoding();
  void setEncoding(const QString &encoding);
  QString keytab();
  void setKeytab(const QString &keytab);
  QSize size();
  void setSize(QSize size);
  void setFont(const QString &font);
  QString font();

public Q_SLOTS:

  void run();
  void done();
  void done(int);
  void terminate();
  void setUserTitle( int, const QString &caption );
  void changeTabTextColor( int );
  void ptyError();
  void slotZModemDetected();
  void emitZModemDetected();

  void zmodemStatus(KProcess *, char *data, int len);
  void zmodemSendBlock(KProcess *, char *data, int len);
  void zmodemRcvBlock(const char *data, int len);
  void zmodemDone();
  void zmodemContinue();

Q_SIGNALS:

  void processExited();
  void receivedData( const QString& text );
  void done(TESession*);
  void updateTitle();
  void notifySessionState(TESession* session, int state);
  void changeTabTextColor( TESession*, int );

  void disableMasterModeConnections();
  void enableMasterModeConnections();
  void renameSession(TESession* ses, const QString &name);

  void openUrlRequest(const QString &cwd);

  void zmodemDetected(TESession *);
  void updateSessionConfig(TESession *);
  void resizeSession(TESession *session, QSize size);
  void setSessionEncoding(TESession *session, const QString &encoding);

// SPLIT-VIEW Disabled
//  void getSessionSchema(TESession *session, QString &schema);
//  void setSessionSchema(TESession *session, const QString &schema);

private Q_SLOTS:
  void onRcvBlock( const char* buf, int len );
  void monitorTimerDone();
  void notifySessionState(int state);
  void onContentSizeChange(int height, int width);
  //automatically detach views from sessions when view is destroyed
  void viewDestroyed(QObject* view);

private:

  void updateTerminalSize();

  int            _uniqueIdentifier;

  TEPty*         _shellProcess;
  TEmulation*    _emulation;

  QList<TEWidget*> _views;

  bool           connected;
  bool           monitorActivity;
  bool           monitorSilence;
  bool           notifiedActivity;
  bool           masterMode;
  bool           autoClose;
  bool           wantedClose;
  QTimer*        monitorTimer;

  //FIXME: using the indices here
  // is propably very bad. We should
  // use a persistent reference instead.
  //
  int            _fontNo;
  int            _silenceSeconds;

  QString        _title;
  QString        _userTitle;
  QString        _iconName;
  QString        _iconText; // as set by: echo -en '\033]1;IconText\007
  bool           _addToUtmp;
  bool           _flowControl;
  bool           _fullScripting;

  QString	 stateIconName;

  QString        _program;
  QStringList    _arguments;

  QString        term;
  ulong          winId;
  int           _sessionId;

  QString        cwd;
  QString        initial_cwd;

  // ZModem
  bool           zmodemBusy;
  KProcIO*       zmodemProc;
  ZModemDialog*  zmodemProgress;

  // Color/Font Changes by ESC Sequences

  QColor         modifiedBackground; // as set by: echo -en '\033]11;Color\007
  int            encoding_no;

  ColorSchema*   _colorScheme;
  
  static int lastSessionId;
  
};

#endif
