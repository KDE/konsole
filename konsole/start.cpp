/*
    Copyright (C) 2006 by Robert Knight <robertknight@gmail.com>

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

// KDE
#include <KAboutData>
#include <KCmdLineArgs>
#include <KLocale>

#include "KonsoleApp.h"

#define KONSOLE_VERSION "2.0"

extern "C" int KDE_EXPORT kdemain(int argc,char** argv)
{
    KAboutData about("konsole",i18n("Konsole").toLocal8Bit().data(),KONSOLE_VERSION);
    KCmdLineArgs::init(argc,argv,&about);
    KUniqueApplication::addCmdLineOptions();

    // create a new application instance if there are no running Konsole instances,
    // otherwise inform the existing Konsole instance and exit
    if ( !KUniqueApplication::start() )
    {
        exit(0);
    }

    KonsoleApp app;
    return app.exec();    
}
