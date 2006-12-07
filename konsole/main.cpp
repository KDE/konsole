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

// Standard Library / Unix
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

// Qt
#include <QDir>
#include <QSessionManager>
#include <QWidget>
#include <QStringList>

// X11
#if defined(Q_WS_X11) && defined(HAVE_XRENDER)
    #define TRUE_TRANSPARENCY_SUPPORT
#endif

#ifdef TRUE_TRANSPARENCY_SUPPORT
    #include <X11/Xlib.h>
    #include <X11/extensions/Xrender.h>
    #include <fixx11h.h>
#endif

// KDE
#include <klocale.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kdebug.h>
#include <kstandarddirs.h>
#include <kglobalsettings.h>
#include <kio/netaccess.h>
#include <kmessagebox.h>
#include <config.h>
#include <kauthorized.h>
#include <ksimpleconfig.h>

// Konsole
#include "konsole.h"
#include "SessionManager.h"


static const char description[] =
  I18N_NOOP("The KDE Terminal");

static KCmdLineOptions options[] =
{
   { "name <name>",    I18N_NOOP("Set window class"), 0 },
   { "ls",             I18N_NOOP("Start login shell"), 0 },
   { "T <title>",      I18N_NOOP("Set the window title"), 0 },
   { "tn <terminal>",  I18N_NOOP("Specify terminal type as set in the TERM\nenvironment variable"), "xterm" },
   { "noclose",        I18N_NOOP("Do not close Konsole when command exits"), 0 },
   { "nohist",         I18N_NOOP("Do not save lines in history"), 0 },
   { "nomenubar",      I18N_NOOP("Do not display menubar"), 0 },
   { "notabbar",       0, 0 },
   { "notoolbar",      I18N_NOOP("Do not display tab bar"), 0 },
   { "noframe",        I18N_NOOP("Do not display frame"), 0 },
   { "noscrollbar",    I18N_NOOP("Do not display scrollbar"), 0 },
   { "noxft",          I18N_NOOP("Do not use Xft (anti-aliasing)"), 0 },
   { "vt_sz CCxLL",    I18N_NOOP("Terminal size in columns x lines"), 0 },
   { "noresize",       I18N_NOOP("Terminal size is fixed"), 0 },
   { "type <type>",    I18N_NOOP("Start with given session type"), 0 },
   { "types",          I18N_NOOP("List available session types"), 0 },
   { "keytab <name>",  I18N_NOOP("Set keytab to 'name'"), 0 },
   { "keytabs",        I18N_NOOP("List available keytabs"), 0 },
   { "profile <name>", I18N_NOOP("Start with given session profile"), 0 },
   { "profiles",       I18N_NOOP("List available session profiles"), 0 },
   { "schema <name> | <file>",  I18N_NOOP("Set schema to 'name' or use 'file'"), 0 },
   { "schemas",        0, 0 },
   { "schemata",       I18N_NOOP("List available schemata"), 0 },
//   { "script",         I18N_NOOP("Enable extended DCOP Qt functions"), 0 },
   { "workdir <dir>",  I18N_NOOP("Change working directory to 'dir'"), 0 },
   { "!e <command>",   I18N_NOOP("Execute 'command' instead of shell"), 0 },
   // WABA: All options after -e are treated as arguments.
   { "+[args]",        I18N_NOOP("Arguments for 'command'"), 0 },
   KCmdLineLastOption
};

static bool has_noxft = false;
static bool login_shell = false;
//static bool full_script = false;
static bool auto_close = true;
static bool fixed_size = false;
bool true_transparency = false;


//prototypes
const char* konsole_shell(QStringList& args);

// Retrieve information about display needed to enable true transparency.  This will
// search for a display, visual format and colormap which can be passed to the KApplication
// constructor to enable the use of true transparency in the application.  If compositing
// support is not available then 'display', 'visual' and 'colormap' will be set to null. 
// 
// displayName - The name of the display to use, will use the default display
//               if this is an empty string.
// display     - Out parameter. Receives a pointer to the X display to use
// visual      - Out parameter. Receives a pointer to the X visual to use
// colormap    - Out parameter. Set to the X colormap to use

#ifdef TRUE_TRANSPARENCY_SUPPORT
void getDisplayInfo( const QString& displayName , Display*& display , 
                     Visual*& visual, Colormap& colormap);
#endif


/**
   The goal of this class is to add "--noxft" and "--ls" to the restoreCommand
   if konsole was started with any of those options.
 */
class KonsoleSessionManaged: public KSessionManaged {
public:
    bool saveState( QSessionManager&sm) {
        QStringList restartCommand = sm.restartCommand();
        if (has_noxft)
            restartCommand.append("--noxft");
        if (login_shell)
            restartCommand.append("--ls");
//        if (full_script)
//            restartCommand.append("--script");
        if (!auto_close)
            restartCommand.append("--noclose");
        if (fixed_size)
            restartCommand.append("--noresize");
        sm.setRestartCommand(restartCommand);
        return true;
    }
};



/* --| main |------------------------------------------------------ */
extern "C" int KDE_EXPORT kdemain(int argc, char* argv[])
{
  setgid(getgid()); setuid(getuid()); // drop privileges

  // deal with shell/command 
  bool histon = true;
  bool menubaron = true;
  bool tabbaron = true;
  bool frameon = true;
  bool scrollbaron = true;
  bool showtip = true;
  QByteArray wname = PACKAGE;


  KAboutData aboutData( "konsole" , I18N_NOOP("Konsole") , VERSION , description , 
                        KAboutData::License_GPL , I18N_NOOP("(C) 1997-2006 , Konsole Developers") );
  aboutData.addAuthor("Robert Knight",I18N_NOOP("Maintainer"), "robertknight@gmail.com");
  aboutData.addAuthor("Lars Doelle",I18N_NOOP("Author"), "lars.doelle@on-line.de");
  aboutData.addCredit("Kurt V. Hindenburg",
    I18N_NOOP("Bug fixes and general improvements"), 
    "kurt.hindenburg@gmail.com");
  aboutData.addCredit("Waldo Bastian",
    I18N_NOOP("Bug fixes and general improvements"),
    "bastian@kde.org");
  aboutData.addCredit("Stephan Binner",
    I18N_NOOP("Bug fixes and general improvements"),
    "binner@kde.org");
  aboutData.addCredit("Chris Machemer",
    I18N_NOOP("Bug fixes"),
    "machey@ceinetworks.com");
  aboutData.addCredit("Stephan Kulow",
    I18N_NOOP("Solaris support and history"),
    "coolo@kde.org");
  aboutData.addCredit("Alexander Neundorf",
    I18N_NOOP("Bug fixes and improved startup performance"),
    "neundorf@kde.org");
  aboutData.addCredit("Peter Silva",
    I18N_NOOP("Marking improvements"),
    "peter.silva@videotron.ca");
  aboutData.addCredit("Lotzi Boloni",
    I18N_NOOP("Embedded Konsole\n"
    "Toolbar and session names"),
    "boloni@cs.purdue.edu");
  aboutData.addCredit("David Faure",
    I18N_NOOP("Embedded Konsole\n"
    "General improvements"),
    "David.Faure@insa-lyon.fr");
  aboutData.addCredit("Antonio Larrosa",
    I18N_NOOP("Visual effects"),
    "larrosa@kde.org");
  aboutData.addCredit("Matthias Ettrich",
    I18N_NOOP("Code from the kvt project\n"
    "General improvements"),
    "ettrich@kde.org");
  aboutData.addCredit("Warwick Allison",
    I18N_NOOP("Schema and text selection improvements"),
    "warwick@troll.no");
  aboutData.addCredit("Dan Pilone",
    I18N_NOOP("SGI port"),
    "pilone@slac.com");
  aboutData.addCredit("Kevin Street",
    I18N_NOOP("FreeBSD port"),
    "street@iname.com");
  aboutData.addCredit("Sven Fischer",
    I18N_NOOP("Bug fixes"),
    "herpes@kawo2.rwth-aachen.de");
  aboutData.addCredit("Dale M. Flaven",
    I18N_NOOP("Bug fixes"),
    "dflaven@netport.com");
  aboutData.addCredit("Martin Jones",
    I18N_NOOP("Bug fixes"),
    "mjones@powerup.com.au");
  aboutData.addCredit("Lars Knoll",
    I18N_NOOP("Bug fixes"),
    "knoll@mpi-hd.mpg.de");
  aboutData.addCredit("",I18N_NOOP("Thanks to many others.\n"));

  KCmdLineArgs::init( argc, argv, &aboutData );
  KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.
  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
  KCmdLineArgs *qtargs = KCmdLineArgs::parsedArgs("qt");
  has_noxft = !args->isSet("xft");
  TEWidget::setAntialias( !has_noxft );
  TEWidget::setStandalone( true );

  // The following Qt options have no effect; warn users.
  if( qtargs->isSet("background") )
      kWarning() << "The Qt option -bg, --background has no effect." << endl;
  if( qtargs->isSet("foreground") )
      kWarning() << "The Qt option -fg, --foreground has no effect." << endl;
  if( qtargs->isSet("button") )
      kWarning() << "The Qt option -btn, --button has no effect." << endl;
  if( qtargs->isSet("font") )
      kWarning() << "The Qt option -fn, --font has no effect." << endl;

#ifdef TRUE_TRANSPARENCY_SUPPORT
  Colormap colormap  = 0;
  Visual*  visual    = 0;
  Display* display   = 0;

  getDisplayInfo( qtargs->getOption("display") , display , visual , colormap );
  KApplication a( display , Qt::HANDLE( visual ), Qt::HANDLE( colormap ) );
#else
  KApplication a;
#endif

  //create session manager to keep track of available session types
  //and active terminal sessions
  SessionManager* sessionManager = new SessionManager();
  
  
  QString title;
  if(args->isSet("T")) {
    title = QFile::decodeName(args->getOption("T"));
  }
  if(qtargs->isSet("title")) {
    title = QFile::decodeName(qtargs->getOption("title"));
  }

  QString term = "";
  if(args->isSet("tn")) {
    term=QLatin1String(args->getOption("tn"));
  }
  login_shell = args->isSet("ls");

  QStringList eargs;

  const char* shell = 0;
  if (!args->getOption("e").isEmpty())
  {
     if (args->isSet("ls"))
        KCmdLineArgs::usage(i18n("You can not use BOTH -ls and -e.\n"));
     shell = strdup(args->getOption("e"));
     eargs.append(shell);
     for(int i=0; i < args->count(); i++)
       eargs.append( args->arg(i) );

     if (title.isEmpty() &&
         (kapp->caption() == kapp->aboutData()->programName()))
     {
        title = QFile::decodeName(shell);  // program executed in the title bar
     }
     showtip = false;
  }

  QByteArray sz = "";
  sz = args->getOption("vt_sz");
  histon = args->isSet("hist");
  menubaron = args->isSet("menubar");
  tabbaron = args->isSet("tabbar") && args->isSet("toolbar");
  frameon = args->isSet("frame");
  scrollbaron = args->isSet("scrollbar");
  wname = qtargs->getOption("name");
//  full_script = args->isSet("script");
  auto_close = args->isSet("close");
  fixed_size = !args->isSet("resize");
#ifdef __GNUC__
#warning "KDE4: Does DBUS have an equivalent function for dcopClient()->setQtBridgeEnabled()?"
#endif  
#if 0
  if (!full_script)
	a.dcopClient()->setQtBridgeEnabled(false);
#endif
  QByteArray type = "";

  if(args->isSet("type")) {
    type = args->getOption("type");
  }
  if(args->isSet("types")) {
    
    QListIterator<SessionInfo*> infoIter(sessionManager->availableSessionTypes());
    while (infoIter.hasNext())
    {
        printf("%s\n",QFileInfo(infoIter.next()->path()).baseName().toAscii().data());
    }
    
    return 0;
  }
  if(args->isSet("schemas") || args->isSet("schemata")) {
    ColorSchemaList colors;
    colors.checkSchemas();
    for(int i = 0; i < (int) colors.count(); i++)
    {
       ColorSchema *schema = colors.find(i);
       QString relPath = schema->relPath();
       if (!relPath.isEmpty())
          printf("%s\n", QFile::encodeName(relPath).data());
    }
    return 0;
  }

  if(args->isSet("keytabs")) {
    QStringList lst = KGlobal::dirs()->findAllResources("data", "konsole/*.keytab");

    printf("default\n");   // 'buildin' keytab
    lst.sort();
    for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it )
    {
      QFileInfo fi(*it);
      QString file = fi.baseName();
      printf("%s\n", QFile::encodeName(file).data());
    }
    return 0;
  }

  QString workDir = QFile::decodeName( args->getOption("workdir") );

  QString keytab = "";
  if (args->isSet("keytab"))
    keytab = QFile::decodeName(args->getOption("keytab"));

  QString schema = "";
  if (args->isSet("schema"))
    schema = args->getOption("schema");

  KConfig * sessionconfig = 0;
  QString profile = "";
  if (args->isSet("profile")) {
    profile = args->getOption("profile");
    QString path = KStandardDirs::locate( "data", "konsole/profiles/" + profile );
    if ( QFile::exists( path ) )
      sessionconfig=new KConfig( path, true );
    else
      profile = "";
  }
  if (args->isSet("profiles"))
  {
     QStringList profiles = KGlobal::dirs()->findAllResources("data", "konsole/profiles/*", false, true);
     profiles.sort();
     for(QStringList::ConstIterator it = profiles.begin();
         it != profiles.end(); ++it)
     {
        QString file = *it;
        file = file.mid(file.lastIndexOf('/')+1);
        printf("%s\n", QFile::encodeName(file).data());
     }
     return 0;
  }

  args->clear();

  int c = 0, l = 0;
  if ( !sz.isEmpty() )
  {
    char *ls = (char*)strchr( sz.data(), 'x' );
    if ( ls != NULL )
    {
       *ls='\0';
       ls++;
       c=atoi(sz.data());
       l=atoi(ls);
    }
    else
    {
       KCmdLineArgs::usage(i18n("expected --vt_sz <#columns>x<#lines> e.g. 80x40\n"));
    }
  }

  if (!KAuthorized::authorizeKAction("size"))
    fixed_size = true;

  // ///////////////////////////////////////////////

  // Ignore SIGHUP so that we don't get killed when
  // our parent-shell gets closed.
  signal(SIGHUP, SIG_IGN);

  putenv((char*)"COLORTERM="); // to trigger mc's color detection
  KonsoleSessionManaged ksm;

  if (a.isSessionRestored() || !profile.isEmpty())
  {
    if (!shell)
       shell = konsole_shell(eargs);

    if (profile.isEmpty())
      sessionconfig = a.sessionConfig();
    sessionconfig->setDesktopGroup();
    int n = 1;

    QString key;
    QString sTitle;
    QString sPgm;
    QString sTerm;
    QString sIcon;
    QString sCwd;
    int     n_tabbar;

    // TODO: Session management stores everything in same group,
    // should use one group / mainwindow
    while (KMainWindow::canBeRestored(n) || !profile.isEmpty())
    {
        sessionconfig->setGroup(QString("%1").arg(n));
        if (!sessionconfig->hasKey("Pgm0"))
            sessionconfig->setDesktopGroup(); // Backwards compatible

        int session_count = sessionconfig->readEntry("numSes").toInt();
        int counter = 0;

        wname = sessionconfig->readEntry("class",QString(wname)).toLatin1();

        sPgm = sessionconfig->readEntry("Pgm0", shell);
        QStringList eargs_sl = sessionconfig->readEntry("Args0", QStringList());
        for (int i = 0; i < eargs_sl.size(); ++i)
           eargs.append(eargs_sl.at(i).toAscii());

        sTitle = sessionconfig->readEntry("Title0", title);
        sTerm = sessionconfig->readEntry("Term0");
        sIcon = sessionconfig->readEntry("Icon0","konsole");
        sCwd = sessionconfig->readPathEntry("Cwd0");
        workDir = sessionconfig->readPathEntry("workdir");
        n_tabbar = qMin(sessionconfig->readEntry("tabbar", QVariant(Konsole::TabBottom)).toUInt(), 2u);
        Konsole *m = new Konsole(wname,histon,menubaron,tabbaron,frameon,scrollbaron,0/*type*/,true,n_tabbar, workDir);


        m->setSessionManager(sessionManager);

        kDebug() << "WARNING:  Not using appropriate properties for new session" << endl;
        m->newSession( sessionManager->defaultSessionType() );
        
        //m->newSession(sPgm, eargs, sTerm, sIcon, sTitle, sCwd);

//        m->enableFullScripting(full_script);
        m->enableFixedSize(fixed_size);
	m->restore(n);
        sessionconfig->setGroup(QString("%1").arg(n));
        if (!sessionconfig->hasKey("Pgm0"))
            sessionconfig->setDesktopGroup(); // Backwards compatible
        m->makeGUI();
        m->setEncoding(sessionconfig->readEntry("Encoding0", QVariant(0)).toInt());
        m->setSchema(sessionconfig->readEntry("Schema0"));
        // Use konsolerc default as tmpFont instead?
        QVariant v_defaultFont = sessionconfig->readEntry("SessionFont0", QVariant(KGlobalSettings::fixedFont()));
        m->initSessionFont( v_defaultFont.value<QFont>() );
        m->initSessionKeyTab(sessionconfig->readEntry("KeyTab0"));
        m->initMonitorActivity(sessionconfig->readEntry("MonitorActivity0", QVariant(false)).toBool());
        m->initMonitorSilence(sessionconfig->readEntry("MonitorSilence0", QVariant(false)).toBool());
        m->initMasterMode(sessionconfig->readEntry("MasterMode0", QVariant(false)).toBool());
        //FIXME: Verify this code when KDE4 supports tab colors... kvh
        QVariant v_tabColor = sessionconfig->readEntry("TabColor0");
        m->initTabColor( v_tabColor.value<QColor>() );
        // -1 will be changed to the default history in konsolerc
//        m->initHistory(sessionconfig->readNumEntry("History0", -1),
//                       sessionconfig->readBoolEntry("HistoryEnabled0", true));

        counter++;

        // show() before 2nd+ sessions are created allows --profile to
        //  initialize the TE size correctly.
        m->show();

        while (counter < session_count)
        {
          key = QString("Title%1").arg(counter);
          sTitle = sessionconfig->readEntry(key, title);
          key = QString("Args%1").arg(counter);
          QStringList eargs_sl = sessionconfig->readEntry("Args0",QStringList());
          for (int i = 0; i < eargs_sl.size(); ++i)
             eargs.append(eargs_sl.at(i).toAscii());

          key = QString("Pgm%1").arg(counter);
          sPgm = sessionconfig->readEntry(key, QString(shell));
          key = QString("Term%1").arg(counter);
          sTerm = sessionconfig->readEntry(key, QString());
          key = QString("Icon%1").arg(counter);
          sIcon = sessionconfig->readEntry(key,QString("konsole"));
          key = QString("Cwd%1").arg(counter);
          sCwd = sessionconfig->readPathEntry(key);
          
          //TODO: Make use of command, program type, icon etc. here
          m->newSession();
          
          //m->newSession(sPgm, eargs, sTerm, sIcon, sTitle, sCwd);
          
          m->setSessionTitle(sTitle);  // Use title as is
          key = QString("Schema%1").arg(counter);
          m->setSchema(sessionconfig->readEntry(key, QString()));
          key = QString("Encoding%1").arg(counter);
          m->setEncoding(sessionconfig->readEntry(key, QVariant(0)).toInt());
          key = QString("SessionFont%1").arg(counter);
          QVariant v_defaultFont = sessionconfig->readEntry(key, QVariant(KGlobalSettings::fixedFont()));
          m->initSessionFont( v_defaultFont.value<QFont>() );

          key = QString("KeyTab%1").arg(counter);
          m->initSessionKeyTab(sessionconfig->readEntry(key, QString()));
          key = QString("MonitorActivity%1").arg(counter);
          m->initMonitorActivity(sessionconfig->readEntry(key, QVariant(false)).toBool());
          key = QString("MonitorSilence%1").arg(counter);
          m->initMonitorSilence(sessionconfig->readEntry(key, QVariant(false)).toBool());
          key = QString("MasterMode%1").arg(counter);
          m->initMasterMode(sessionconfig->readEntry(key, QVariant(false)).toBool());
          key = QString("TabColor%1").arg(counter);
//          m->initTabColor(sessionconfig->readColorEntry(key));
          //FIXME: Verify this code when KDE4 supports tab colors... kvh
//          QVariant v_tabColor = sessionconfig->readEntry(key));
//          m->initTabColor( v_tabColor.value<QColor>() );

/* Test this when the dialogs work again...kvh
          // -1 will be changed to the default history in konsolerc
          key = QString("History%1").arg(counter);
          QString key2 = QString("HistoryEnabled%1").arg(counter);
          m->initHistory(sessionconfig->readNumEntry(key, -1),
                         sessionconfig->readBoolEntry(key2, true));
*/

          counter++;
        }
        m->setDefaultSession( sessionconfig->readEntry("DefaultSession","shell.desktop") );

        m->initFullScreen();
        if ( !profile.isEmpty() ) {
          m->callReadPropertiesInternal(sessionconfig,1);
          profile = "";
          // Hack to work-around sessions initialized with minimum size
          for (int i=0;i<counter;i++)
            m->activateSession( i );
          m->setColLin(c,l); // will use default height and width if called with (0,0)
        }
	// works only for the first one, but there won't be more.
        n++;
        m->activateSession( sessionconfig->readEntry("ActiveSession", QVariant(0)).toInt() );
	m->setAutoClose(auto_close);
    }
  }
  else
  {
    Konsole*  m = new Konsole(wname,histon,menubaron,tabbaron,frameon,scrollbaron,type, false, 0, workDir);

   m->setSessionManager(sessionManager);

   kDebug() <<  "* WARNING - Code to create new session not done yet" << endl;
   //TODO : Make use of session properties
   m->newSession();
   
 //   m->newSession((shell ? QFile::decodeName(shell) : QString()), eargs, term, QString(), title, workDir);
//    m->enableFullScripting(full_script);
    m->enableFixedSize(fixed_size);

    if (!keytab.isEmpty())
      m->initSessionKeyTab(keytab);

    if (!schema.isEmpty()) {
      if (schema.right(7)!=".schema")
        schema+=".schema";
      m->setSchema(schema);
      m->activateSession(0); // Fixes BR83162, transp. schema + notabbar
    }

    m->setColLin(c,l); // will use default height and width if called with (0,0)

    m->initFullScreen();
    m->show();
    if (showtip)
      m->showTipOnStart();
    m->setAutoClose(auto_close);
  }

  int ret = a.exec();

  return ret;
}

const char *konsole_shell(QStringList &args)
{
  const char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";
  if (login_shell)
  {
    char* t = (char*)strrchr(shell,'/');
    if (t) // see sh(1)
    {
      t = strdup(t);
      *t = '-';
      args.append(t);
      free(t);
    }
    else
      args.append(shell);
  }
  else
    args.append(shell);
  return shell;
}


#ifdef TRUE_TRANSPARENCY_SUPPORT
void getDisplayInfo( const QString& displayName , Display*& dpy , Visual*& visual, Colormap& colormap )
{
  dpy = 0;
  visual = 0;
  colormap = 0;

  char *display = 0;

  if ( !displayName.isEmpty() )
      display = displayName.toAscii().data();

  dpy = XOpenDisplay( display );
  if ( !dpy ) {
    kError() << "cannot connect to X server " << display << endl;
    exit( 1 );
  }

  int screen = DefaultScreen( dpy );
  int event_base, error_base;

  if ( XRenderQueryExtension( dpy, &event_base, &error_base ) )
  {
    int nvi;
    XVisualInfo templ;
    templ.screen  = screen;
    templ.depth   = 32;
    templ.c_class = TrueColor;
    XVisualInfo *xvi = XGetVisualInfo( dpy, VisualScreenMask | VisualDepthMask
		  | VisualClassMask, &templ, &nvi );

    for ( int i = 0; i < nvi; i++ ) {
      XRenderPictFormat *format = XRenderFindVisualFormat( dpy, xvi[i].visual );
      if ( format->type == PictTypeDirect && format->direct.alphaMask ) {
        visual = xvi[i].visual;
        colormap = XCreateColormap( dpy, RootWindow( dpy, screen ), visual, AllocNone );
        true_transparency = true;
        break;
      }
    }
  }
}
#endif
