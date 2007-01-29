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

#include <kmenu.h>
#include <kstandarddirs.h>
#include <kshell.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <kdebug.h>
#include <kbookmarkmenu.h>
#include <QFile>

#include "konsole.h"
#include "konsolebookmarkhandler.h"

KonsoleBookmarkHandler::KonsoleBookmarkHandler( Konsole *konsole, bool toplevel )
    : QObject( konsole ),
      KBookmarkOwner(),
      m_konsole( konsole ),
      m_toplevel(toplevel)
{
    setObjectName( "KonsoleBookmarkHandler" );

    m_menu = new KMenu( konsole );

    // KDE3.5 - Konsole's bookmarks are now in konsole/bookmarks.xml
    // TODO: Consider removing for KDE4
    QString new_bm_file = KStandardDirs::locateLocal( "data", "konsole/bookmarks.xml" );
    if ( !QFile::exists( new_bm_file ) ) {
        QString old_bm_file = KStandardDirs::locateLocal( "data", "kfile/bookmarks.xml" );
        if ( QFile::exists( old_bm_file ) )
            // We want sync here...
            if ( !KIO::NetAccess::file_copy( KUrl( old_bm_file ),
                                   KUrl ( new_bm_file ), 0 ) ) {
                kWarning()<<KIO::NetAccess::lastErrorString()<<endl;
            }
    }

    m_file = KStandardDirs::locate( "data", "konsole/bookmarks.xml" );
    if ( m_file.isEmpty() )
        m_file = KStandardDirs::locateLocal( "data", "konsole/bookmarks.xml" );

    KBookmarkManager *manager = KBookmarkManager::managerForFile( m_file, "konsole", false);
    manager->setEditorOptions(KGlobal::caption(), false);
    manager->setUpdate( true );

    if (toplevel) {
        m_bookmarkMenu = new KBookmarkMenu( manager, this, m_menu,
                                            konsole->actionCollection() );
    } else {
        m_bookmarkMenu = new KBookmarkMenu( manager, this, m_menu,
                                            NULL);
    }
}

KonsoleBookmarkHandler::~KonsoleBookmarkHandler()
{
    delete m_bookmarkMenu;
}

void KonsoleBookmarkHandler::openBookmark( const KBookmark & bm, Qt::MouseButtons, Qt::KeyboardModifiers )
{
    emit openUrl( bm.url().url(), bm.text() );
}

bool KonsoleBookmarkHandler::addBookmarkEntry() const
{
    return m_toplevel;
}

bool KonsoleBookmarkHandler::editBookmarkEntry() const
{
    return m_toplevel;
}

QString KonsoleBookmarkHandler::currentUrl() const
{
    return m_konsole->baseURL().prettyUrl();
}

QString KonsoleBookmarkHandler::currentTitle() const
{
    const KUrl &u = m_konsole->baseURL();
    if (u.isLocalFile())
    {
       QString path = u.path();
       path = KShell::tildeExpand(path);
       return path;
    }
    return u.prettyUrl();
}

#include "konsolebookmarkhandler.moc"
