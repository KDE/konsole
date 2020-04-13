/*  This file was part of the KDE libraries

    Copyright 2019 by Tomaz Canabrava <tcanabrava@kde.org>
    Copyright 2019 by Martin Sandsmark <martin.sandsmark@kde.org>

    library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation, version 2
    or ( at your option ), any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

// Own
#include "BookmarkMenu.h"
#include "Shortcut_p.h"

// KDE
#include <KActionCollection>
#include <kbookmarks_version.h>
#include <KBookmarkManager>
#include <KBookmark>

// Qt
#include <QAction>
#include <QMenu>

#include <algorithm>    // std::any_of

BookmarkMenu::BookmarkMenu (KBookmarkManager *mgr, KBookmarkOwner *owner, QMenu *parentMenu, KActionCollection *collection) :
#if KBOOKMARKS_VERSION < QT_VERSION_CHECK(5, 69, 0)
    KBookmarkMenu (mgr, owner, parentMenu, collection)
#else
    KBookmarkMenu (mgr, owner, parentMenu)
#endif
{
    QAction *bookmarkAction;
#if KBOOKMARKS_VERSION < QT_VERSION_CHECK(5, 69, 0)
    bookmarkAction = collection->action(QStringLiteral("add_bookmark"));
#else
    collection->addActions(parentMenu->actions());

    bookmarkAction = addBookmarkAction();
#endif

    Q_ASSERT(bookmarkAction);

    // We need to hijack the action - note this only hijacks top-level action
    disconnect(bookmarkAction, nullptr, this, nullptr);
    connect(bookmarkAction, &QAction::triggered, this, &BookmarkMenu::maybeAddBookmark);

    // replace Ctrl+B shortcut for bookmarks only if user hasn't already
    // changed the shortcut; however, if the user changed it to Ctrl+B
    // this will still get changed to Ctrl+Shift+B
    if (bookmarkAction->shortcut() == QKeySequence(Konsole::ACCEL + Qt::Key_B)) {
        collection->setDefaultShortcut(bookmarkAction, Konsole::ACCEL + Qt::SHIFT + Qt::Key_B);
    }
}

void BookmarkMenu::maybeAddBookmark()
{
    // Check for duplicates first
    // This only catches duplicates in top-level (ie not sub-folders);
    //  this is due to only the top-level add_bookmark is hijacked.
    const KBookmarkGroup rootGroup = manager()->root();
    const QUrl currUrl = owner()->currentUrl();
    const auto urlList = rootGroup.groupUrlList();
    if (std::any_of(urlList.constBegin(), urlList.constEnd(),
                   [currUrl] (const QUrl &url) { return url == currUrl; })) {
        return;
    }

    slotAddBookmark();
}
