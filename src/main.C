/* ---------------------------------------------------------------------- */
/*                                                                        */
/* [main.C]                        Konsole                                */
/*                                                                        */
/* ---------------------------------------------------------------------- */
/*                                                                        */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>        */
/*                                                                        */
/* This file is part of Konsole, an X terminal.                           */
/*                                                                        */
/* The material contained in here more or less directly orginates from    */
/* kvt, which is copyright (c) 1996 by Matthias Ettrich <ettrich@kde.org> */
/*                                                                        */
/* ---------------------------------------------------------------------- */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include <qdir.h>

#include <kapp.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kimageio.h>
#include <kdebug.h>
#include <dcopclient.h>
#include "konsole.h"

static const char *description =
  I18N_NOOP("X terminal for use with KDE.");

//   { "T <title>",       0, 0 },
static KCmdLineOptions options[] =
{
   { "name <name>",  I18N_NOOP("Set Window Class"), 0 },
   { "ls",    I18N_NOOP("Start login shell"), 0 },
   { "nowelcome",       I18N_NOOP("Suppress greeting"), 0 },
   { "T", 0, 0 },
   { "title <title>",           I18N_NOOP("Set the window title"), 0 },
   { "xwin",            I18N_NOOP("ignored"), 0 },
   { "nohist",          I18N_NOOP("Do not save lines in scroll-back buffer"), 0 },
   { "notoolbar",       I18N_NOOP("Do not display toolbar"), 0 },
   { "vt_sz CCxLL",  I18N_NOOP("Terminal size in columns x lines"), 0 },
   { "type <type>", I18N_NOOP("Open the given session type instead of the default shell"), 0 },
   { "workdir <dir>", I18N_NOOP("Change working directory of the konsole to 'dir'"), 0 },
   { "!e <command>",  I18N_NOOP("Execute 'command' instead of shell"), 0 },
   // WABA: All options after -e are treated as arguments.
   { "+[args]",    I18N_NOOP("Arguments for 'command'"), 0 },
   { 0, 0, 0 }
};


/**
   The goal of this class is to prevent GUI confirmation
   on the exit when the class is called from the session
   manager.
   It must be here, because this has to be called before
   the KMainWindow's manager.
 */
class KonsoleSessionManaged: public KSessionManaged {
public:
    bool commitData( QSessionManager&) {
        konsole->skip_exit_query = true;
        return true;
    };
    Konsole *konsole;
};



/* --| main |------------------------------------------------------ */
int main(int argc, char* argv[])
{
  setuid(getuid()); setgid(getgid()); // drop privileges

  // deal with shell/command ////////////////////////////
  bool login_shell = false;
  bool welcome = true;
  bool histon = true;
  bool toolbaron = true;
  const char* wname = PACKAGE;


  KAboutData aboutData( PACKAGE, I18N_NOOP("Konsole"),
    VERSION, description, KAboutData::License_GPL_V2,
    "(c) 1997-2001, Lars Doelle");
  aboutData.addAuthor("Waldo Bastian",I18N_NOOP("Maintainer"), "bastian@kde.org");
  aboutData.addAuthor("Lars Doelle",I18N_NOOP("Author"), "lars.doelle@on-line.de");
  aboutData.addCredit("Chris Machemer",
    I18N_NOOP("bug fixing"),
    "machey@ceinetworks.com");
  aboutData.addCredit("Peter Silva",
    I18N_NOOP("decent marking"),
    "peter.silva@videotron.ca");
  aboutData.addCredit("Lotzi Boloni",
    I18N_NOOP("partification\n"
    "Toolbar and session names"),
    "boloni@cs.purdue.edu");
  aboutData.addCredit("David Faure",
    I18N_NOOP("partification\n"
    "overall improvements"),
    "David.Faure@insa-lyon.fr");
  aboutData.addCredit("Antonio Larrosa",
    I18N_NOOP("transparency"),
    "larrosa@kde.org");
  aboutData.addCredit("Matthias Ettrich",
    I18N_NOOP("most of main.C donated via kvt\n"
    "overall improvements"),
    "ettrich@kde.org");
  aboutData.addCredit("Warwick Allison",
    I18N_NOOP("schema and selection improvements"),
    "warwick@troll.no");
  aboutData.addCredit("Dan Pilone",
    I18N_NOOP("SGI Port"),
    "pilone@slac.com");
  aboutData.addCredit("Kevin Street",
    I18N_NOOP("FreeBSD port"),
    "street@iname.com");
  aboutData.addCredit("Sven Fischer",
    I18N_NOOP("bug fixing"),
    "herpes@kawo2.rwth-aachen.de");
  aboutData.addCredit("Dale M. Flaven",
    I18N_NOOP("bug fixing"),
    "dflaven@netport.com");
  aboutData.addCredit("Martin Jones",
    I18N_NOOP("bug fixing"),
    "mjones@powerup.com.au");
  aboutData.addCredit("Lars Knoll",
    I18N_NOOP("bug fixing"),
    "knoll@mpi-hd.mpg.de");
  aboutData.addCredit("Alexander Neundorf",
    I18N_NOOP("faster startup, bug fixing"),
    "neundorf@kde.org");
  aboutData.addCredit("Stephan Kulow",
    I18N_NOOP("Solaris support and work on history"),
    "coolo@kde.org");
  aboutData.addCredit("",I18N_NOOP("Thanks to many others.\n"
    "The above list only reflects the contributors\n"
    "I managed to keep track of."));

  KCmdLineArgs::init( argc, argv, &aboutData );
  KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.
  //1.53 sec

  KApplication a;
  KImageIO::registerFormats(); // add io for additional image formats
  //2.1 secs

  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
  QString title;
  if(args->isSet("T")) {
    title = QFile::decodeName(args->getOption("title"));
  }

  QStrList eargs;

  const char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";
  if (args->getOption("e").isEmpty())
  {
    char* t = (char*)strrchr(shell,'/');
    if (args->isSet("ls") && t) // see sh(1)
    {
      t = strdup(t); *t = '-';
      //KONSOLEDEBUG << "Appending t = " << t << " to eargs." << endl;
      //eargs.append(shell);
      eargs.append(t);
    }
    else
      eargs.append(shell);
  }
  else
  {
     if (args->isSet("ls"))
        KCmdLineArgs::usage(i18n("You can't use BOTH -ls and -e.\n"));
     shell = strdup(args->getOption("e"));
     eargs.clear();
     eargs.append(shell);
     for(int i=0; i < args->count(); i++)
       eargs.append( args->arg(i) );

     if (title.isEmpty() && 
         (kapp->caption() == kapp->aboutData()->programName()))
     {
        title = QFile::decodeName(shell);  // program executed in the title bar
     }
  }

  if (title.isEmpty())
  {
     title = kapp->caption(); // `konsole' or -caption
  }

  QCString sz = "";
  sz = args->getOption("vt_sz");
  histon = args->isSet("hist");
  toolbaron = args->isSet("toolbar");
  wname = args->getOption("name");
  login_shell = args->isSet("ls");
  welcome = args->isSet("welcome");

  QCString type = "";

  if(args->isSet("type")) {
    type = args->getOption("type");
  }
  if(args->isSet("workdir"))
     QDir::setCurrent( args->getOption("workdir") );
 
  //FIXME: more: font, menu, scrollbar, schema, session ...

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
       KCmdLineArgs::usage(i18n("expected --vt_sz <#columns>x<#lines> ie. 80x40\n"));
    }
  }

  // ///////////////////////////////////////////////

  // a.dcopClient()->registerAs(a.name()); //FIXME: Experimental

  // ///////////////////////////////////////////////

  putenv((char*)"COLORTERM="); // to trigger mc's color detection
  KonsoleSessionManaged *ksm = new KonsoleSessionManaged();

  if (a.isRestored())
  {
    KConfig * sessionconfig = a.sessionConfig();
    sessionconfig->setDesktopGroup();
    wname = sessionconfig->readEntry("class",wname).latin1();
//    RESTORE( Konsole(wname,shell,eargs,histon,toolbaron) )
    int n = 1;

    int session_count = sessionconfig->readNumEntry("numSes");
    int counter = 0;
    QString key;
    QString sTitle;
    QString sPgm;

    while (KMainWindow::canBeRestored(n))
    {
        sessionconfig->setDesktopGroup();
        sPgm = sessionconfig->readEntry("Pgm0", shell);
        sessionconfig->readListEntry("Args0", eargs);
        sTitle = sessionconfig->readEntry("Title0", title);
        Konsole *m = new Konsole(wname,sPgm,eargs,histon,toolbaron,sTitle,type,true);
        m->restore(n);
        m->makeGUI();
        m->initSessionSchema(sessionconfig->readNumEntry("Schema0"));
        counter++;

        while (counter < session_count) 
        {
          sessionconfig->setDesktopGroup();
          key = QString("Title%1").arg(counter);
          sTitle = sessionconfig->readEntry(key, title);
          key = QString("Args%1").arg(counter);
          sessionconfig->readListEntry(key, eargs);
          key = QString("Pgm%1").arg(counter);
          sPgm = sessionconfig->readEntry(key, shell);
          m->newSession(sPgm, eargs);
          m->initRenameSession(sTitle);
          key = QString("Schema%1").arg(counter);
          m->initSessionSchema(sessionconfig->readNumEntry(key));
          counter++;
        }
        ksm->konsole = m;
        ksm->konsole->initFullScreen();
        // works only for the first one, but there won't be more.
        n++;
    }
  }
  else
  {
    //2.1 sec
    Konsole*  m = new Konsole(wname,QFile::decodeName(shell),eargs,histon,toolbaron,title,type);
    //2.5 sec
    ksm->konsole = m;
    m->setColLin(c,l); // will use default height and width if called with (0,0)

    if (welcome && false) // ME: disable the greeting, it mixes up the taskbar
    {
      m->setCaption(i18n("Welcome to the console"));
      QTimer::singleShot(5000,m,SLOT(updateTitle()));
    }
    //2.5 sec
    m->initFullScreen();
    m->show();
  }
  //2.6 sec

  
  return a.exec();
}
