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
    BookmarkMenu (KBookmarkManager *mgr, KBookmarkOwner *owner, QMenu *parentMenu, KActionCollection *collec);

private Q_SLOTS:
    void maybeAddBookmark();
};

#endif//BOOKMARKMENU_H
