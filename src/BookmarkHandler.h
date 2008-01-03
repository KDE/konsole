/* This file wass part of the KDE libraries
    Copyright (C) 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation, version 2 
    or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

// Born as kdelibs/kio/kfile/kfilebookmarkhandler.h

#ifndef KONSOLEBOOKMARKHANDLER_H
#define KONSOLEBOOKMARKHANDLER_H

// Qt
#include <QtGui/QMenu>

// KDE
#include <KBookmarkManager>

class KMenu;
class KBookmarkMenu;
class KBookmarkManager;
class KActionCollection;

namespace Konsole
{

class ViewProperties;

/**
 * This class handles the communication between the bookmark menu and the active session,
 * providing a suggested title and URL when the user clicks the "Add Bookmark" item in
 * the bookmarks menu.
 *
 * The bookmark handler is associated with a session controller, which is used to 
 * determine the working URL of the current session.  When the user changes the active
 * view, the bookmark handler's controller should be changed using setController()
 *
 * When the user selects a bookmark, the openUrl() signal is emitted.
 */
class BookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:

    /**
     * Constructs a new bookmark handler for Konsole bookmarks.
     *
     * @param collection The collection which the boomark menu's actions should be added to
     * @param menu The menu which the bookmark actions should be added to
     * @param toplevel TODO: Document me
     */
    BookmarkHandler( KActionCollection* collection , KMenu* menu, bool toplevel , QObject* parent );
    ~BookmarkHandler();

    QMenu * popupMenu();

    virtual QString currentUrl() const;
    virtual QString currentTitle() const;
    virtual bool enableOption(BookmarkOption option) const;
    virtual bool supportsTabs() const;
    virtual QList<QPair<QString,QString> > currentBookmarkList() const;
	virtual void openFolderinTabs(const KBookmarkGroup& group);

    /** 
     * Returns the menu which this bookmark handler inserts its actions into.
     */
    KMenu *menu() const { return m_menu; }

    QList<ViewProperties*> views() const;
    ViewProperties* activeView() const;

public slots:
    /**
     *
     */
    void setViews( const QList<ViewProperties*>& views );

    void setActiveView( ViewProperties* view );

signals:
    /**
     * Emitted when the user selects a bookmark from the bookmark menu.
     *
     * @param url The url of the bookmark which was selected by the user.
     * @param text TODO: Document me
     */
    void openUrl( const KUrl& url ); 

	/**
	 * Emitted when the user selects 'Open Folder in Tabs' 
	 * from the bookmark menu.
	 *
	 * @param urls The urls of the bookmarks in the folder whoose
	 * 'Open Folder in Tabs' action was triggered
	 */
	void openUrls( const QList<KUrl>& urls );

private Q_SLOTS:
    void openBookmark( const KBookmark & bm, Qt::MouseButtons, Qt::KeyboardModifiers );

private:
    QString titleForView( ViewProperties* view ) const;
    QString urlForView( ViewProperties* view ) const;

    KMenu* m_menu;
    KBookmarkMenu* m_bookmarkMenu;
    QString m_file;
    bool m_toplevel;
    ViewProperties* m_activeView;
    QList<ViewProperties*> m_views;
};

}

#endif // KONSOLEBOOKMARKHANDLER_H
