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
#include <kapp.h>
#include <klocale.h>
#include <kaboutdata.h>
#include <kcmdlineargs.h>
#include <kimageio.h>
#include <dcopclient.h>
#include "konsole.h"

static const char *description =
  I18N_NOOP("X terminal for use with KDE.");

static KCmdLineOptions options[] =
{
   { "name <name>",  I18N_NOOP("Set Window Class"), 0 },
   { "ls",    I18N_NOOP("Start login shell"), 0 },
   { "nowelcome",       I18N_NOOP("Suppress greeting"), 0 },
   { "title",           I18N_NOOP("Set the window title"), 0 },
   { "xwin",            I18N_NOOP("ignored"), 0 },  
   { "nohist",          I18N_NOOP("Do not save lines in scroll-back buffer"), 0 },
   { "vt_sz CCxLL",  I18N_NOOP("Terminal size in columns x lines"), 0 },
   { "!e <command>",  I18N_NOOP("Execute 'command' instead of shell"), 0 },
   // WABA: All options after -e are treated as arguments.
   { "+[args]",    I18N_NOOP("Arguments for 'command'"), 0 },
   { 0, 0, 0 }
};


/* --| main |------------------------------------------------------ */
int main(int argc, char* argv[])
{
  setuid(getuid()); setgid(getgid()); // drop privileges

  // deal with shell/command ////////////////////////////
  bool login_shell = false;
  bool welcome = true;
  bool histon = true;
  const char* wname = PACKAGE;

  KAboutData aboutData( PACKAGE, I18N_NOOP("Konsole"),
    VERSION, description, KAboutData::License_GPL_V2,
    "(c) 1997-2000, Lars Doelle");
  aboutData.addAuthor("Lars Doelle",0, "lars.doelle@on-line.de");
  aboutData.addCredit("Peter Silva",
    "decent marking",
    "peter.silva@videotron.ca");
  aboutData.addCredit("Lotzi Boloni",
    "partification\n"
    "Toolbar and session names",
    "boloni@cs.purdue.edu");
  aboutData.addCredit("David Faure",
    "partification\n"
    "overall improvements",
    "David.Faure@insa-lyon.fr");
  aboutData.addCredit("Antonio Larrosa",
    "transparency",
    "larrosa@kde.org");
  aboutData.addCredit("Matthias Ettrich",
    "most of main.C donated via kvt\n"
    "overall improvements",
    "ettrich@kde.org");
  aboutData.addCredit("Warwick Allison",
    "schema and selection improvements",
    "warwick@troll.no");
  aboutData.addCredit("Dan Pilone",
    "SGI Port",
    "pilone@slac.com");
  aboutData.addCredit("Kevin Street",
    "FreeBSD port",
    "street@iname.com");
  aboutData.addCredit("Sven Fischer",
    "bug fixing",
    "herpes@kawo2.rwth-aachen.de");
  aboutData.addCredit("Dale M. Flaven",
    "bug fixing",
    "dflaven@netport.com");
  aboutData.addCredit("Martin Jones",
    "bug fixing",
    "mjones@powerup.com.au");
  aboutData.addCredit("Lars Knoll",
    "bug fixing",
    "knoll@mpi-hd.mpg.de");
  aboutData.addCredit("Thanks to many other",
    "The above list only reflects the contributors\n"
    "i mananged to keep track for.");
  
  KCmdLineArgs::init( argc, argv, &aboutData );
  KCmdLineArgs::addCmdLineOptions( options ); // Add our own options.

  KApplication a;
  KImageIO::registerFormats(); // add io for additional image formats

  KCmdLineArgs *args = KCmdLineArgs::parsedArgs();
  QStrList eargs;

  const char* shell = getenv("SHELL");
  if (shell == NULL || *shell == '\0') shell = "/bin/sh";
  if (args->getOption("e").isEmpty())
  {
    char* t = (char*)strrchr(shell,'/');
    if (args->isSet("ls") && t) // see sh(1)
    {
      t = strdup(t); *t = '-';
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
  }


  QCString sz = "";
  sz = args->getOption("vt_sz");
  histon = args->isSet("hist");
  wname = args->getOption("name");
  login_shell = args->isSet("ls");
  welcome = args->isSet("welcome");

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

  if (a.isRestored())
  {
    KConfig * sessionconfig = a.sessionConfig();
    sessionconfig->setGroup("options");
    sessionconfig->readListEntry("konsolearguments", eargs);
    wname = sessionconfig->readEntry("class",wname).latin1();
    RESTORE( Konsole(wname,shell,eargs,histon) )
  }
  else
  {
    Konsole*  m = new Konsole(wname,shell,eargs,histon);
    m->setColLin(c,l); // will use default height and width if called with (0,0)

    if (welcome)
    {
      m->setCaption(i18n("Welcome to the console"));
      QTimer::singleShot(5000,m,SLOT(setHeader()));
    }
    m->show();
  }

  return a.exec();
}
