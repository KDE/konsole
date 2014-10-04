/* This file was part of the KDE libraries

    Copyright 2002 Carsten Pfeiffer <pfeiffer@kde.org>
    Copyright 2007-2008 Robert Knight <robertknight@gmail.com>

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

#ifndef BOOKMARKHANDLER_H
#define BOOKMARKHANDLER_H

// KDE
#include <KBookmarkManager>

#include <QtCore/QUrl>

// Konsole
#include "konsole_export.h"

class KMenu;
class QMenu;
class KBookmarkMenu;
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
class KONSOLEPRIVATE_EXPORT BookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    /**
     * Constructs a new bookmark handler for Konsole bookmarks.
     *
     * @param collection The collection which the bookmark menu's actions should be added to
     * @param menu The menu which the bookmark actions should be added to
     * @param toplevel TODO: Document me
     * @param parent The parent object
     */
    BookmarkHandler(KActionCollection* collection , QMenu* menu, bool toplevel , QObject* parent);
    ~BookmarkHandler();

    virtual QUrl currentUrl() const;
    virtual QString currentTitle() const;
    virtual QString currentIcon() const;
    virtual bool enableOption(BookmarkOption option) const;
    virtual bool supportsTabs() const;
    virtual QList<KBookmarkOwner::FutureBookmark> currentBookmarkList() const;
    virtual void openFolderinTabs(const KBookmarkGroup& group);

    /**
     * Returns the menu which this bookmark handler inserts its actions into.
     */
    QMenu* menu() const {
        return _menu;
    }

    QList<ViewProperties*> views() const;
    ViewProperties* activeView() const;

public slots:
    /**
     *
     */
    void setViews(const QList<ViewProperties*>& views);

    void setActiveView(ViewProperties* view);

signals:
    /**
     * Emitted when the user selects a bookmark from the bookmark menu.
     *
     * @param url The url of the bookmark which was selected by the user.
     */
    void openUrl(const QUrl& url);

    /**
     * Emitted when the user selects 'Open Folder in Tabs'
     * from the bookmark menu.
     *
     * @param urls The urls of the bookmarks in the folder whose
     * 'Open Folder in Tabs' action was triggered
     */
    void openUrls(const QList<QUrl>& urls);

private slots:
    void openBookmark(const KBookmark& bm, Qt::MouseButtons, Qt::KeyboardModifiers);

private:
    QString titleForView(ViewProperties* view) const;
    QUrl urlForView(ViewProperties* view) const;
    QString iconForView(ViewProperties* view) const;

    QMenu* _menu;
    KBookmarkMenu* _bookmarkMenu;
    QString _file;
    bool _toplevel;
    ViewProperties* _activeView;
    QList<ViewProperties*> _views;
};
}

#endif // BOOKMARKHANDLER_H
