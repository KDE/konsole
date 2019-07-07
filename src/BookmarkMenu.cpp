
// Own
#include <BookmarkMenu.h>

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
