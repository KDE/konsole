/*  This file was part of the KDE libraries
    
    Copyright 2002 Carsten Pfeiffer <pfeiffer@kde.org>
    Copyright 2007 Robert Knight <robertknight@gmail.com> 

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

// Qt
#include <QFile>

// KDE
#include <kshell.h>

#include <KBookmarkMenu>
#include <KDebug>
#include <KIO/Job>
#include <KIO/NetAccess>
#include <KMenu>
#include <KStandardDirs>

// Konsole
#include "KonsoleBookmarkHandler.h"
#include "SessionController.h"

KonsoleBookmarkHandler::KonsoleBookmarkHandler( KActionCollection* collection, KMenu* menu, bool toplevel )
    : QObject( collection ),
      KBookmarkOwner(),
      m_toplevel(toplevel),
      m_controller( 0 )
{
    setObjectName( "KonsoleBookmarkHandler" );

    m_menu = menu;

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
    
    manager->setUpdate( true );

    if (toplevel) {
        m_bookmarkMenu = new KBookmarkMenu( manager, this, m_menu,
                                            collection );
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
    if ( m_controller )
    {
        return m_controller->url().prettyUrl();
    }
    else
    {
        return QString(); 
    }
}

QString KonsoleBookmarkHandler::currentTitle() const
{
    const KUrl &u = m_controller ? m_controller->url() : KUrl(); 
    if (u.isLocalFile())
    {
       QString path = u.path();
       path = KShell::tildeExpand(path);
       return path;
    }
    return u.prettyUrl();
}

void KonsoleBookmarkHandler::setController( SessionController* controller ) 
{
    m_controller = controller;
}
SessionController* KonsoleBookmarkHandler::controller() const
{
    return m_controller;
}

#include "KonsoleBookmarkHandler.moc"
