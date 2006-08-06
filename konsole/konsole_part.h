/* -*- C++ -*-
    This file is part of the KDE system
    Copyright (C)  1999,2000 Boloni Laszlo <lboloni@cpe.ucf.edu>

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#ifndef KONSOLE_PART_H
#define KONSOLE_PART_H

#include <kparts/browserextension.h>
#include <kparts/factory.h>


#include <kdialogbase.h>

#include <kde_terminal_interface.h>

#include "schema.h"
#include "session.h"

class KInstance;
class konsoleBrowserExtension;
class QPushButton;
class QSpinBox;
class KPopupMenu;
class KActionMenu;
class QCheckBox;
class KRootPixmap;
class KToggleAction;
class KSelectAction;

namespace KParts { class GUIActivateEvent; }

class konsoleFactory : public KParts::Factory
{
    Q_OBJECT
public:
    konsoleFactory();
    virtual ~konsoleFactory();

    virtual KParts::Part* createPartObject(QWidget *parentWidget = 0, const char *widgetName = 0,
                                     QObject* parent = 0, const char* name = 0,
                                     const char* classname = "KParts::Part",
                                     const QStringList &args = QStringList());

    static KInstance *instance();

 private:
    static KInstance *s_instance;
    static KAboutData *s_aboutData;
};

//////////////////////////////////////////////////////////////////////

class konsolePart: public KParts::ReadOnlyPart, public TerminalInterface, public ExtTerminalInterface
{
    Q_OBJECT
	public:
    konsolePart(QWidget *parentWidget, const char *widgetName, QObject * parent, const char *name, const char *classname = 0);
    virtual ~konsolePart();

signals:
    void processExited( KProcess * );
    void receivedData( const QString& s );
    void forkedChild();
 protected:
    virtual bool openURL( const KURL & url );
    virtual bool openFile() {return false;} // never used
    virtual bool closeURL() {return true;}
    virtual void guiActivateEvent( KParts::GUIActivateEvent * event );

 protected slots:
    void showShell();

    void doneSession(TESession*);
    void sessionDestroyed();
    void configureRequest(TEWidget*,int,int x,int y);
    void updateTitle(TESession*);
    void enableMasterModeConnections();

 private slots:
    void emitOpenURLRequest(const QString &url);

    void readProperties();
    void saveProperties();
    void applyProperties();
    void setSettingsMenuEnabled( bool );

    void sendSignal(int n);
    void closeCurrentSession();

    void notifySize(int /*columns*/, int /*lines*/);

    void slotToggleFrame();
    void slotSelectScrollbar();
    void slotSelectFont();
    void schema_menu_check();
    void keytab_menu_activated(int item);
    void updateSchemaMenu();
    void setSchema(int n);
    void pixmap_menu_activated(int item);
    void schema_menu_activated(int item);
    void slotHistoryType();
    void slotSelectBell();
    void slotSelectLineSpacing();
    void slotBlinkingCursor();
    void slotUseKonsoleSettings();
    void slotWordSeps();
    void slotSetEncoding();
    void biggerFont();
    void smallerFont();

    void autoShowShell();

 private:
    konsoleBrowserExtension *m_extension;
    KURL currentURL;

    void makeGUI();
    void applySettingsToGUI();

    void setSchema(ColorSchema* s);
    void updateKeytabMenu();

	bool doOpenStream( const QString& );
	bool doWriteStream( const QByteArray& );
	bool doCloseStream();

    QWidget* parentWidget;
    TEWidget* te;
    TESession* se;
    ColorSchemaList* colors;
    KRootPixmap* rootxpm;

    KActionCollection* actions;
    KActionCollection* settingsActions;

    KToggleAction* blinkingCursor;
    KToggleAction* showFrame;
    KToggleAction* m_useKonsoleSettings;

    KSelectAction* selectBell;
    KSelectAction* selectLineSpacing;
    KSelectAction* selectScrollbar;
    KSelectAction* selectSetEncoding;

    KActionMenu* m_fontsizes;

    KPopupMenu* m_keytab;
    KPopupMenu* m_schema;
    KPopupMenu* m_signals;
    KPopupMenu* m_options;
    KPopupMenu* m_popupMenu;

    QFont       defaultFont;

    QString     pmPath; // pixmap path
    QString     s_schema;
    QString     s_kconfigSchema;
    QString     s_word_seps;			// characters that are considered part of a word

    bool        b_framevis:1;
    bool        b_histEnabled:1;
    bool        b_useKonsoleSettings:1;
    bool        b_autoDestroy:1;
    bool        b_autoStartShell:1;

    int         curr_schema; // current schema no
    int         n_bell;
    int         n_keytab;
    int         n_render;
    int         n_scroll;
    unsigned    m_histSize;
    bool        m_runningShell;
    bool        m_streamEnabled;
    int         n_encoding;

public:
    // these are the implementations for the TermEmuInterface
    // functions...
    void startProgram( const QString& program,
                       const QStrList& args );
    void newSession();
    void showShellInDir( const QString& dir );
    void sendInput( const QString& text );
    void setAutoDestroy( bool );
    void setAutoStartShell( bool );
};

//////////////////////////////////////////////////////////////////////

class HistoryTypeDialog : public KDialogBase
{
    Q_OBJECT
public:
  HistoryTypeDialog(const HistoryType& histType,
                    unsigned int histSize,
                    QWidget *parent);

public slots:
  void slotDefault();
  void slotSetUnlimited();
  void slotHistEnable(bool);

  unsigned int nbLines() const;
  bool isOn() const;

protected:
  QCheckBox* m_btnEnable;
  QSpinBox*  m_size;
  QPushButton* m_setUnlimited;
};

//////////////////////////////////////////////////////////////////////

class konsoleBrowserExtension : public KParts::BrowserExtension
{
    Q_OBJECT
	friend class konsolePart;
 public:
    konsoleBrowserExtension(konsolePart *parent);
    virtual ~konsoleBrowserExtension();

    void emitOpenURLRequest(const KURL &url);
};

#endif
