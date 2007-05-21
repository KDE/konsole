/*
    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>

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

// Own
#include "Application.h"

// X11
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>

// KDE
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>
#include <KWindowSystem>

// bump the version to 2.0 before the KDE 4 release
#define KONSOLE_VERSION "1.9"

using namespace Konsole;

// fills the KAboutData structure with information about contributors to 
// Konsole
void fillAboutData(KAboutData& aboutData);
void getDisplayInformation(Display*& display , Visual*& visual , Colormap& colormap,
                           bool& transparencyAvailable);

static KCmdLineOptions options[] =
{
    { "background-mode" , I18N_NOOP("TODO: More documentation") , 0 }
};

// ***
// 
// Entry point into the Konsole terminal application.  
//
// If another Konsole instance
// is already executing then the existing instance is asked to create a new main
// window and the current process returns immediately.
//
// ***
extern "C" int KDE_EXPORT kdemain(int argc,char** argv)
{
    KAboutData about(   "konsole",
                        I18N_NOOP("Konsole"),
                        KONSOLE_VERSION,
                        I18N_NOOP("Terminal emulator for KDE"),
                        KAboutData::License_GPL_V2
                    );
    fillAboutData(about);

    KCmdLineArgs::init(argc,argv,&about);
    KCmdLineArgs::addCmdLineOptions(options);
    KUniqueApplication::addCmdLineOptions();

    // create a new application instance if there are no running Konsole instances,
    // otherwise inform the existing Konsole instance and exit
    if ( !KUniqueApplication::start() )
    {
        exit(0);
    }

    //Display* display = 0;
    Display* display = XOpenDisplay(0);
    Visual* visual = 0;
    Colormap colormap = 0;
    bool transparencyAvailable = false;

    //getDisplayInformation(display,visual,colormap,transparencyAvailable);

    qDebug() << "Transparency available: " << transparencyAvailable;

    Application app(display,Qt::HANDLE(visual),Qt::HANDLE(colormap));
    return app.exec();   
}

void fillAboutData(KAboutData& aboutData)
{
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
    "peter.silva@videotron.Character");
  aboutData.addCredit("Lotzi Boloni",
    I18N_NOOP("Embedded Konsole\n"
    "Toolbar and session names"),
    "boloni@cs.purdue.edu");
  aboutData.addCredit("David Faure",
    I18N_NOOP("Embedded Konsole\n"
    "General improvements"),
    "David.Faure@insa-lyon.foregroundColorr");
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
    "herpes@kawo2.renditionwth-aachen.de");
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

}

// code taken from the Qt 4 graphics dojo examples 
void getDisplayInformation(Display*& display , Visual*& visual , Colormap& colormap,
                           bool& transparencyAvailable)
{
    display = XOpenDisplay(0); // open default display
    if (!display) {
        qWarning("Cannot connect to the X server");
        exit(1);
    }

    int screen = DefaultScreen(display);
    int eventBase, errorBase;

    if (XRenderQueryExtension(display, &eventBase, &errorBase)) {
        int nvi;
        XVisualInfo templ;
        templ.screen  = screen;
        templ.depth = 32;
        templ.c_class = TrueColor;
        XVisualInfo *xvi = XGetVisualInfo(display, VisualScreenMask |
                                          VisualDepthMask |
                                          VisualClassMask, &templ, &nvi);
    
        for (int i = 0; i < nvi; ++i) {
            XRenderPictFormat* format = XRenderFindVisualFormat(display,
                                                                xvi[i].visual);
            if (format->type == PictTypeDirect && format->direct.alphaMask) {
                visual = xvi[i].visual;
                colormap = XCreateColormap(display, RootWindow(display, screen),
                                           visual, AllocNone);

                transparencyAvailable = true;

                // found ARGB visual
                break;
            }
        }
    }
}
