/* This file wass part of the KDE libraries
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

// Born as kdelibs/kio/kfile/kfilebookmarkhandler.h

#ifndef KONSOLEBOOKMARKHANDLER_H
#define KONSOLEBOOKMARKHANDLER_H

#include <KBookmarkManager>

class KMenu;
class KBookmarkMenu;
class KBookmarkManager;
class KMainWindow;

class SessionController;

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
class KonsoleBookmarkHandler : public QObject, public KBookmarkOwner
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
    KonsoleBookmarkHandler( KActionCollection* collection , KMenu* menu, bool toplevel );
    ~KonsoleBookmarkHandler();

    QMenu * popupMenu();

    virtual QString currentUrl() const;
    virtual QString currentTitle() const;
    virtual bool addBookmarkEntry() const;
    virtual bool editBookmarkEntry() const;

    /** 
     * Returns the menu which this bookmark handler inserts its actions into.
     */
    KMenu *menu() const { return m_menu; }

    /**
     * Sets the controller used to retrieve the current session URL when
     * the "Add Bookmark" menu item is selected.
     */
    void setController( SessionController* controller );
    /**
     * Returns the controller used to retrieve the current session URL when
     * the "Add Bookmark" menu item is selected.
     */
    SessionController* controller() const;

Q_SIGNALS:
    /**
     * Emitted when the user selects a bookmark from the bookmark menu.
     *
     * @param url The url of the bookmark which was selected by the user.
     * @param text TODO: Document me
     */
    void openUrl( const QString& url , const QString& text );

private Q_SLOTS:
    void openBookmark( const KBookmark & bm, Qt::MouseButtons, Qt::KeyboardModifiers );

private:
    KMenu* m_menu;
    KBookmarkMenu* m_bookmarkMenu;
    QString m_file;
    bool m_toplevel;
    SessionController* m_controller;
};

#endif // KONSOLEBOOKMARKHANDLER_H
