/*
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

// Qt
#include <QStringList>

// KDE
#include <kparts/browserextension.h>
#include <kparts/factory.h>
#include <kdialog.h>
#include <kde_terminal_interface.h>

class KInstance;
class konsoleBrowserExtension;
class QPushButton;
class QSpinBox;
class KMenu;
class KActionMenu;
class QCheckBox;
class KToggleAction;
class KSelectAction;
class ColorSchema;
class ColorSchemaList;
class HistoryType;
class KAboutData;
class TESession;
class TEWidget;

namespace KParts { class GUIActivateEvent; }

class konsoleFactory : public KParts::Factory
{
    Q_OBJECT
public:
    konsoleFactory();
    virtual ~konsoleFactory();

    virtual KParts::Part* createPartObject(QWidget *parentWidget = 0,
                                     QObject* parent = 0,
                                     const char* classname = "KParts::Part",
                                     const QStringList &args = QStringList());

    static KInstance *instance();

 private:
    static KInstance *s_instance;
    static KAboutData *s_aboutData;
};

//////////////////////////////////////////////////////////////////////

class konsolePart: public KParts::ReadOnlyPart, public TerminalInterface
{
    Q_OBJECT
	public:
    konsolePart(QWidget *parentWidget, QObject * parent, const char *classname = 0);
    virtual ~konsolePart();

Q_SIGNALS:
    void processExited();
    void receivedData( const QString& s );
 protected:
    virtual bool openUrl( const KUrl & url );
    virtual bool openFile() {return false;} // never used
    virtual bool closeUrl() {return true;}
    virtual void guiActivateEvent( KParts::GUIActivateEvent * event );

 protected Q_SLOTS:
    void showShell();
    void slotProcessExited();
    void slotReceivedData( const QString& s );

    void doneSession(TESession*);
    void sessionDestroyed();
    void configureRequest(TEWidget*,int,int x,int y);
    void updateTitle();
    void enableMasterModeConnections();

 private Q_SLOTS:
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

 private:
    konsoleBrowserExtension *m_extension;
    KUrl currentURL;

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

    KMenu* m_keytab;
    KMenu* m_schema;
    KMenu* m_signals;
    KMenu* m_options;
    KMenu* m_popupMenu;

    QFont       defaultFont;

    QString     pmPath; // pixmap path
    QString     s_schema;
    QString     s_kconfigSchema;
    QString     s_word_seps;			// characters that are considered part of a word

    bool        b_framevis:1;
    bool        b_histEnabled:1;
    bool        b_useKonsoleSettings:1;

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
                       const QStringList& args );
    void showShellInDir( const QString& dir );
    void sendInput( const QString& text );
};

//////////////////////////////////////////////////////////////////////

class HistoryTypeDialog : public KDialog
{
    Q_OBJECT
public:
  HistoryTypeDialog(const HistoryType& histType,
                    unsigned int histSize,
                    QWidget *parent);

public Q_SLOTS:
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

    void emitOpenURLRequest(const KUrl &url);
};

#endif
