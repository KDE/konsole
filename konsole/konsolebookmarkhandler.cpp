// Born as kdelibs/kio/kfile/kfilebookmarkhandler.cpp

#include <stdio.h>
#include <stdlib.h>

#include <qtextstream.h>

#include <kbookmarkimporter.h>
#include <kmimetype.h>
#include <kpopupmenu.h>
#include <ksavefile.h>
#include <kstandarddirs.h>

#include "konsole.h"
#include "konsolebookmarkhandler.h"

KonsoleBookmarkHandler::KonsoleBookmarkHandler( Konsole *konsole, bool toplevel )
    : QObject( konsole, "KonsoleBookmarkHandler" ),
      KBookmarkOwner(),
      m_konsole( konsole ),
      m_importStream( 0L )
{
    m_menu = new KPopupMenu( konsole, "bookmark menu" );

    QString file = locate( "data", "kfile/bookmarks.xml" );
    if ( file.isEmpty() )
        file = locateLocal( "data", "kfile/bookmarks.xml" );

    // import old bookmarks
    if ( !KStandardDirs::exists( file ) ) {
        QString oldFile = locate( "data", "kfile/bookmarks.html" );
        if ( !oldFile.isEmpty() )
            importOldBookmarks( oldFile, file );
    }

    KBookmarkManager *manager = KBookmarkManager::managerForFile( file, false);
    manager->setUpdate( true );
    manager->setShowNSBookmarks( false );
    
    connect( manager, SIGNAL( changed(const QString &, const QString &) ),
             SLOT( slotBookmarksChanged(const QString &, const QString &) ) );

    if (toplevel) {
        m_bookmarkMenu = new KBookmarkMenu( manager, this, m_menu,
                                            konsole->actionCollection(), true );
    } else {
        m_bookmarkMenu = new KBookmarkMenu( manager, this, m_menu,
                                            NULL, false /* Not toplevel */
					    ,false      /* No 'Add Bookmark' */);
    }
}

QString KonsoleBookmarkHandler::currentURL() const
{
    return m_konsole->baseURL().url();
}

void KonsoleBookmarkHandler::importOldBookmarks( const QString& path,
                                                 const QString& destinationPath )
{
    KSaveFile file( destinationPath );
    if ( file.status() != 0 )
        return;

    m_importStream = file.textStream();
    *m_importStream << "<!DOCTYPE xbel>\n<xbel>\n";

    KNSBookmarkImporter importer( path );
    connect( &importer,
             SIGNAL( newBookmark( const QString&, const QCString&, const QString& )),
             SLOT( slotNewBookmark( const QString&, const QCString&, const QString& )));
    connect( &importer,
             SIGNAL( newFolder( const QString&, bool, const QString& )),
             SLOT( slotNewFolder( const QString&, bool, const QString& )));
    connect( &importer, SIGNAL( newSeparator() ), SLOT( newSeparator() ));
    connect( &importer, SIGNAL( endMenu() ), SLOT( endMenu() ));

    importer.parseNSBookmarks( false );

    *m_importStream << "</xbel>";

    file.close();
    m_importStream = 0L;
}

void KonsoleBookmarkHandler::slotNewBookmark( const QString& /*text*/,
                                            const QCString& url,
                                            const QString& additionalInfo )
{
    *m_importStream << "<bookmark icon=\"" << KMimeType::iconForURL( url );
    *m_importStream << "\" href=\"" << QString::fromUtf8(url) << "\">\n";
    *m_importStream << "<title>" << (additionalInfo.isEmpty() ? QString::fromUtf8(url) : additionalInfo) << "</title>\n</bookmark>\n";
}

void KonsoleBookmarkHandler::slotNewFolder( const QString& text, bool /*open*/,
                                          const QString& /*additionalInfo*/ )
{
    *m_importStream << "<folder icon=\"bookmark_folder\">\n<title=\"";
    *m_importStream << text << "\">\n";
}

void KonsoleBookmarkHandler::slotBookmarksChanged( const QString &,
                                                   const QString &)
{
    // This is called when someone changes bookmarks in konsole....
    m_bookmarkMenu->slotBookmarksChanged("");
}

void KonsoleBookmarkHandler::newSeparator()
{
    *m_importStream << "<separator/>\n";
}

void KonsoleBookmarkHandler::endFolder()
{
    *m_importStream << "</folder>\n";
}

void KonsoleBookmarkHandler::virtual_hook( int id, void* data )
{ KBookmarkOwner::virtual_hook( id, data ); }

#include "konsolebookmarkhandler.moc"
