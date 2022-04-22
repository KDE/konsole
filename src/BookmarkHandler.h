/* This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2002 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Born as kdelibs/kio/kfile/kfilebookmarkhandler.h

#ifndef BOOKMARKHANDLER_H
#define BOOKMARKHANDLER_H

// KDE
#include <KBookmarkManager>
#include <KBookmarkOwner>

// Konsole
#include "ViewProperties.h"
#include "konsoleprivate_export.h"

class QMenu;
class QUrl;
class KActionCollection;

namespace Konsole
{

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
    BookmarkHandler(KActionCollection *collection, QMenu *menu, bool toplevel, QObject *parent);
    ~BookmarkHandler() override;

    QUrl currentUrl() const override;
    QString currentTitle() const override;
    QString currentIcon() const override;
    bool enableOption(BookmarkOption option) const override;
    bool supportsTabs() const override;
    QList<KBookmarkOwner::FutureBookmark> currentBookmarkList() const override;
    void openFolderinTabs(const KBookmarkGroup &group) override;

    /**
     * Returns the menu which this bookmark handler inserts its actions into.
     */
    QMenu *menu() const
    {
        return _menu;
    }

    QList<ViewProperties *> views() const;
    ViewProperties *activeView() const;

public Q_SLOTS:
    /**
     *
     */
    void setViews(const QList<ViewProperties *> &views);

    void setActiveView(ViewProperties *view);

Q_SIGNALS:
    /**
     * Emitted when the user selects a bookmark from the bookmark menu.
     *
     * @param url The url of the bookmark which was selected by the user.
     */
    void openUrl(const QUrl &url);

    /**
     * Emitted when the user selects 'Open Folder in Tabs'
     * from the bookmark menu.
     *
     * @param urls The urls of the bookmarks in the folder whose
     * 'Open Folder in Tabs' action was triggered
     */
    void openUrls(const QList<QUrl> &urls);

private Q_SLOTS:
    void openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers) override;

private:
    Q_DISABLE_COPY(BookmarkHandler)

    QString titleForView(ViewProperties *view) const;
    QUrl urlForView(ViewProperties *view) const;
    QString iconForView(ViewProperties *view) const;

    QMenu *_menu;
    QString _file;
    bool _toplevel;
    ViewProperties *_activeView;
    QList<ViewProperties *> _views;
};
}

#endif // BOOKMARKHANDLER_H
