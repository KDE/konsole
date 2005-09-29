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
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

// Born as kdelibs/kio/kfile/kfilebookmarkhandler.cpp

#include <kpopupmenu.h>
#include <kstandarddirs.h>
#include <kshell.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <kdebug.h>
#include <qfile.h>

#include "konsole.h"
#include "konsolebookmarkmenu.h"
#include "konsolebookmarkhandler.h"

KonsoleBookmarkHandler::KonsoleBookmarkHandler( Konsole *konsole, bool toplevel )
    : QObject( konsole, "KonsoleBookmarkHandler" ),
      KBookmarkOwner(),
      m_konsole( konsole )
{
    m_menu = new KPopupMenu( konsole, "bookmark menu" );

    // KDE3.5 - Konsole's bookmarks are now in konsole/bookmarks.xml
    // TODO: Consider removing for KDE4
    QString new_bm_file = locateLocal( "data", "konsole/bookmarks.xml" );
    if ( !QFile::exists( new_bm_file ) ) {
        QString old_bm_file = locateLocal( "data", "kfile/bookmarks.xml" );
        if ( QFile::exists( old_bm_file ) )
            // We want sync here... 
            if ( !KIO::NetAccess::copy( KURL( old_bm_file ), 
                                   KURL ( new_bm_file ), 0 ) ) {
                kdWarning()<<KIO::NetAccess::lastErrorString()<<endl;
            }
    }

    m_file = locate( "data", "konsole/bookmarks.xml" );
    if ( m_file.isEmpty() )
        m_file = locateLocal( "data", "konsole/bookmarks.xml" );

    KBookmarkManager *manager = KBookmarkManager::managerForFile( m_file, false);
    manager->setEditorOptions(kapp->caption(), false);
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
       path = KShell::tildeExpand(path);
       return path;
    }
    return u.prettyURL();
}

void KonsoleBookmarkHandler::slotBookmarksChanged( const QString &,
                                                   const QString &)
{
    // This is called when someone changes bookmarks in konsole....
    m_bookmarkMenu->slotBookmarksChanged("");
}

#include "konsolebookmarkhandler.moc"
