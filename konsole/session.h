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

#include "TEPty.h"
#include "TEWidget.h"
#include "TEmuVt102.h"

#include "sessioniface.h"

class KProcIO;
class KProcess;
class ZModemDialog;

class TESession : public QObject, virtual public SessionIface
{ Q_OBJECT

public:

  TESession(TEWidget* w,
            const QString &pgm, const QStrList & _args,
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
  void setXonXoff(bool);
  bool testAndSetStateIconName (const QString& newname);
  bool sendSignal(int signal);

  void setAutoClose(bool b) { autoClose = b; }

  // Additional functions for DCOP
  bool closeSession();
  void clearHistory();
  void feedSession(const QString &text);
  void sendSession(const QString &text);
  void renameSession(const QString &name);

  virtual bool processDynamic(const QCString &fun, const QByteArray &data, QCString& replyType, QByteArray &replyData);
  virtual QCStringList functionsDynamic();
  void enableFullScripting(bool b) { fullScripting = b; }

  void startZModem(const QString &rz, const QString &dir, const QStringList &list);
  void cancelZModem();
  bool zmodemIsBusy() { return zmodemBusy; }

  void print(QPainter &paint, bool friendly, bool exact);

public slots:

  void run();
  void done();
  void done(int);
  void terminate();
  void setUserTitle( int, const QString &caption );
  void ptyError();
  void slotZModemDetected();
  void emitZModemDetected();

  void zmodemStatus(KProcess *, char *data, int len);
  void zmodemSendBlock(KProcess *, char *data, int len);
  void zmodemRcvBlock(const char *data, int len);
  void zmodemDone();
  void zmodemContinue();

signals:

  void processExited();
  void receivedData( const QString& text );
  void done(TESession*);
  void updateTitle();
  void notifySessionState(TESession* session, int state);

  void clearAllListenToKeyPress();
  void restoreAllListenToKeyPress();
  void renameSession(TESession* ses, const QString &name);

  void openURLRequest(const QString &cwd);

  void zmodemDetected(TESession *);

private slots:
  void onRcvBlock( const char* buf, int len );
  void monitorTimerDone();
  void notifySessionState(int state);
  void onContentSizeChange(int height, int width);
  void onFontMetricChange(int height, int width);

private:

  TEPty*         sh;
  TEWidget*      te;
  TEmulation*    em;

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
  int            schema_no;
  int            font_no;
  int            silence_seconds;

  int            font_h;
  int            font_w;

  QString        title;
  QString        userTitle;
  QString        iconName;
  QString        iconText; // as set by: echo -en '\033]1;IconText\007
  bool           add_to_utmp;
  bool           xon_xoff;
  bool           fullScripting;

  QString	 stateIconName;

  QString        pgm;
  QStrList       args;

  QString        term;
  QString        sessionId;

  QString        cwd;
  QString        initial_cwd;

  // ZModem
  bool           zmodemBusy;
  KProcIO*       zmodemProc;
  ZModemDialog*  zmodemProgress;
};

#endif
