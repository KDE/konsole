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

// KDE
#include <KActionCollection>

// Qt
#include <QAction>
#include <KBookmarkManager>
#include <KBookmark>

BookmarkMenu::BookmarkMenu (KBookmarkManager *mgr, KBookmarkOwner *owner, QMenu *parentMenu, KActionCollection *collec) :
    KBookmarkMenu (mgr, owner, parentMenu, collec)
{
    // We need to hijack the action
    QAction *bookmarkAction = collec->action(QStringLiteral("add_bookmark"));
    disconnect(bookmarkAction, nullptr, this, nullptr);
    connect(bookmarkAction, &QAction::triggered, this, &BookmarkMenu::maybeAddBookmark);
}

void BookmarkMenu::maybeAddBookmark()
{
    // Check for duplicates first
    const KBookmarkGroup rootGroup = manager()->root();
    const QUrl currUrl = owner()->currentUrl();
    for (const QUrl &url : rootGroup.groupUrlList()) {
        if (url == currUrl) {
            return;
        }
    }

    slotAddBookmark();
}
