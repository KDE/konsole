/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
    SPDX-FileCopyrightText: 2019 Martin Sandsmark <martin.sandsmark@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BOOKMARKMENU_H
#define BOOKMARKMENU_H

// KDE
#include <KBookmarkMenu>

// Konsole
#include "konsoleprivate_export.h"

/* Hackish hack to mitigate a broken behavior of KBookmarkMenu.
 * slotAddBookmark accepts duplicates and it's fragile code,
 * that thing really deserves a rewrite.
 * the easiest way is to "hijack" it's protected method to public
 * and just cast around.
 */
class KONSOLEPRIVATE_EXPORT BookmarkMenu : public KBookmarkMenu
{
    Q_OBJECT

public:
    BookmarkMenu(KBookmarkManager *mgr, KBookmarkOwner *owner, QMenu *parentMenu, KActionCollection *collection);

private Q_SLOTS:
    void maybeAddBookmark();
};

#endif // BOOKMARKMENU_H
