#ifndef KONSOLEBOOKMARKMENU_H
#define KONSOLEBOOKMARKMENU_H

#include <qptrlist.h>
#include <qptrstack.h>
#include <qobject.h>
#include <sys/types.h>
#include <kbookmark.h>
#include <kbookmarkmenu.h>

#include "konsolebookmarkhandler.h"


class QString;
class KBookmark;
class KAction;
class KActionMenu;
class KActionCollection;
class KBookmarkOwner;
class KBookmarkMenu;
class KPopupMenu;
class KonsoleBookmarkMenu;

class KonsoleBookmarkMenu : public KBookmarkMenu
{
    Q_OBJECT

public:
    KonsoleBookmarkMenu( KBookmarkManager* mgr,
                         KonsoleBookmarkHandler * _owner, KPopupMenu * _parentMenu,
                         KActionCollection *collec, bool _isRoot,
                         bool _add = true, const QString & parentAddress = "");

    void fillBookmarkMenu();

public slots:

signals:

private slots:

private:
    KonsoleBookmarkHandler * m_kOwner;

protected slots:
    void slotAboutToShow2();
    void slotBookmarkSelected();
    void slotNSBookmarkSelected();

protected:
    void refill();

private:
    class KonsoleBookmarkMenuPrivate;
    KonsoleBookmarkMenuPrivate *d;
};

#endif // KONSOLEBOOKMARKMENU_H
