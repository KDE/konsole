/* This file was part of the KDE libraries
    Copyright (C) 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation, version 2.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

// Born as kdelibs/kio/kfile/kfilebookmarkhandler.cpp

#include <stdio.h>
#include <stdlib.h>

#include <qdir.h>
#include <qtextstream.h>

#include <kbookmarkimporter.h>
#include <kbookmarkdombuilder.h>
#include <kmimetype.h>
#include <kpopupmenu.h>
#include <ksavefile.h>
#include <kstandarddirs.h>

#include "konsole.h"
#include "konsolebookmarkmenu.h"
#include "konsolebookmarkhandler.h"

KonsoleBookmarkHandler::KonsoleBookmarkHandler( Konsole *konsole, bool toplevel )
    : QObject( konsole, "KonsoleBookmarkHandler" ),
      KBookmarkOwner(),
      m_konsole( konsole )
{
    m_menu = new KPopupMenu( konsole, "bookmark menu" );

    m_file = locate( "data", "kfile/bookmarks.xml" );
    if ( m_file.isEmpty() )
        m_file = locateLocal( "data", "kfile/bookmarks.xml" );

    KBookmarkManager *manager = KBookmarkManager::managerForFile( m_file, false);
    manager->setEditorOptions(kapp->caption(), false);

    // import old bookmarks
    if ( !KStandardDirs::exists( m_file ) ) {
        QString oldFile = locate( "data", "kfile/bookmarks.html" );
        if ( !oldFile.isEmpty() )
            importOldBookmarks( oldFile, manager );
    }

    manager->setUpdate( true );
    manager->setShowNSBookmarks( false );
    
    connect( manager, SIGNAL( changed(const QString &, const QString &) ),
             SLOT( slotBookmarksChanged(const QString &, const QString &) ) );

    if (toplevel) {
        m_bookmarkMenu = new KonsoleBookmarkMenu( manager, this, m_menu,
                                            konsole->actionCollection(), true );
    } else {
        m_bookmarkMenu = new KonsoleBookmarkMenu( manager, this, m_menu,
                                            NULL, false /* Not toplevel */
					    ,false      /* No 'Add Bookmark' */);
    }
}

KonsoleBookmarkHandler::~KonsoleBookmarkHandler()
{
    delete m_bookmarkMenu;
}

void KonsoleBookmarkHandler::slotEditBookmarks()
{
    KProcess proc;
    proc << QString::fromLatin1("keditbookmarks");
    proc << "--nobrowser";
    proc << "--caption" << i18n("Konsole Bookmarks Editor");
    proc << m_file;
    proc.start(KProcess::DontCare);
}

QString KonsoleBookmarkHandler::currentURL() const
{
    return m_konsole->baseURL().prettyURL();
}

QString KonsoleBookmarkHandler::currentTitle() const
{
    const KURL &u = m_konsole->baseURL();
    if (u.isLocalFile())
    {
       QString path = u.path();
       QString home = QDir::homeDirPath();
       if (path.startsWith(home))
          path.replace(0, home.length(), "~");
       return path;
    }
    return u.prettyURL();
}

void KonsoleBookmarkHandler::importOldBookmarks( const QString& path,
                                                 KBookmarkManager *manager )
{
    KBookmarkDomBuilder *builder = new KBookmarkDomBuilder( manager->root(), manager );
    KNSBookmarkImporter importer( path );
    builder->connectImporter( &importer );
    importer.parseNSBookmarks();
    delete builder;
    manager->save();
}

void KonsoleBookmarkHandler::slotBookmarksChanged( const QString &,
                                                   const QString &)
{
    // This is called when someone changes bookmarks in konsole....
    m_bookmarkMenu->slotBookmarksChanged("");
}

#include "konsolebookmarkhandler.moc"
