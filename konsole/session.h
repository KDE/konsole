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

#include <kapplication.h>
#include <kmainwindow.h>
#include <qstrlist.h>

#include <TEPty.h>
#include <TEWidget.h>
#include <TEmuVt102.h>

#include "sessioniface.h"

class TESession : public QObject, virtual public SessionIface
{ Q_OBJECT

public:

  TESession(TEWidget* w,
            const QString &pgm, QStrList & _args,
	    const QString &term, const QString &sessionId="session-1",
	    const QString &initial_cwd = QString::null);
  void changeWidget(TEWidget* w);
  ~TESession();

  void        setConnect(bool r);  // calls setListenToKeyPress(r)
  void        setListenToKeyPress(bool l);
  TEmulation* getEmulation();      // to control emulation
  bool        isSecure();
  bool        isMonitorActivity();
  bool        isMonitorSilence();
  bool        isMasterMode();
  int schemaNo();
  int fontNo();
  const QString& Term();
  const QString& SessionId();
  const QString& Title();
  const QString& IconName();
  const QString& IconText();
  QString fullTitle() const;
  int keymapNo();
  QString keymap();
  QStrList getArgs();
  QString getPgm();
  QString getCwd();
  QString getInitial_cwd() { return initial_cwd; }
  void setInitial_cwd(const QString& _cwd) { initial_cwd=_cwd; }

  void setHistory(const HistoryType&);
  const HistoryType& history();

  void setMonitorActivity(bool);
  void setMonitorSilence(bool);
  void setMonitorSilenceSeconds(int seconds);
  void setMasterMode(bool);
  void setSchemaNo(int sn);
  void setKeymapNo(int kn);
  void setKeymap(const QString& _id);
  void setFontNo(int fn);
  void setTitle(const QString& _title);
  void setIconName(const QString& _iconName);
  void setIconText(const QString& _iconText);
  void setAddToUtmp(bool);
  bool testAndSetStateIconName (const QString& newname);
  bool sendSignal(int signal);

  // Additional functions for DCOP
  bool closeSession();
  void feedSession(const QString &text);
  void sendSession(const QString &text);
  void renameSession(const QString &name);

  virtual bool processDynamic(const QCString &fun, const QByteArray &data, QCString& replyType, QByteArray &replyData);
  virtual QCStringList functionsDynamic();
  void enableFullScripting(bool b) { fullScripting = b; }

public slots:

  void run();
  void done(int status);
  void terminate();
  void setUserTitle( int, const QString &caption );

signals:

  void done(TESession*, int);
  void updateTitle();
  void notifySessionState(TESession* session, int state);

  void clearAllListenToKeyPress();
  void restoreAllListenToKeyPress();
  void renameSession(TESession* ses, const QString &name);

  void openURLRequest(const QString &cwd);

private slots:
  void monitorTimerDone();
  void notifySessionState(int state);

private:

  TEPty*         sh;
  TEWidget*      te;
  TEmulation*    em;

  bool           monitorActivity;
  bool           monitorSilence;
  bool           masterMode;
  QTimer*        monitorTimer;

  //FIXME: using the indices here
  // is propably very bad. We should
  // use a persistent reference instead.
  int            schema_no;
  int            font_no;
  int            silence_seconds;

  QString        title;
  QString        userTitle;
  QString        iconName;
  QString        iconText; // as set by: echo -en '\033]1;IconText\007
  bool           add_to_utmp;
  bool           fullScripting;

  QString	 stateIconName;

  QString        pgm;
  QStrList       args;

  QString        term;
  QString        sessionId;

  QString        cwd;
  QString        initial_cwd;
};

#endif
