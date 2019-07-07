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
