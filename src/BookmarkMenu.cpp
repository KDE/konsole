/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
    SPDX-FileCopyrightText: 2019 Martin Sandsmark <martin.sandsmark@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "BookmarkMenu.h"
#include "Shortcut_p.h"

// KDE
#include <KActionCollection>
#include <KBookmark>
#include <KBookmarkManager>
#include <KBookmarkOwner>

// Qt
#include <QAction>
#include <QMenu>

#include <algorithm> // std::any_of

BookmarkMenu::BookmarkMenu(KBookmarkManager *mgr, KBookmarkOwner *owner, QMenu *parentMenu, KActionCollection *collection)
    : KBookmarkMenu(mgr, owner, parentMenu)
{
    QAction *bookmarkAction;
    collection->addActions(parentMenu->actions());

    bookmarkAction = addBookmarkAction();

    Q_ASSERT(bookmarkAction);

    // We need to hijack the action - note this only hijacks top-level action
    disconnect(bookmarkAction, nullptr, this, nullptr);
    connect(bookmarkAction, &QAction::triggered, this, &BookmarkMenu::maybeAddBookmark);

#ifndef Q_OS_MACOS // not needed on this platform, Cmd+B (shortcut) is different to Ctrl+B (^B in terminal)
    // replace Ctrl+B shortcut for bookmarks only if user hasn't already
    // changed the shortcut; however, if the user changed it to Ctrl+B
    // this will still get changed to Ctrl+Shift+B
    if (bookmarkAction->shortcut() == QKeySequence(Qt::CTRL | Qt::Key_B)) {
        collection->setDefaultShortcut(bookmarkAction, Qt::CTRL | Qt::SHIFT | Qt::Key_B);
    }
#endif
}

void BookmarkMenu::maybeAddBookmark()
{
    // Check for duplicates first
    // This only catches duplicates in top-level (ie not sub-folders);
    //  this is due to only the top-level add_bookmark is hijacked.
    const KBookmarkGroup rootGroup = manager()->root();
    const QUrl currUrl = owner()->currentUrl();
    const auto urlList = rootGroup.groupUrlList();
    if (std::any_of(urlList.constBegin(), urlList.constEnd(), [currUrl](const QUrl &url) {
            return url == currUrl;
        })) {
        return;
    }

    slotAddBookmark();
}
