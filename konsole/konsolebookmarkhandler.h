// Born as kdelibs/kio/kfile/kfilebookmarkhandler.h

#ifndef KONSOLEBOOKMARKHANDLER_H
#define KONSOLEBOOKMARKHANDLER_H

#include <kbookmarkmanager.h>
#include "konsolebookmarkmenu.h"

class QTextStream;
class KPopupMenu;
class KonsoleBookmarkMenu;
class KBookmarkManager;

class KonsoleBookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    KonsoleBookmarkHandler( Konsole *konsole, bool toplevel );
    ~KonsoleBookmarkHandler();

    QPopupMenu * popupMenu();

    // KBookmarkOwner interface:
    virtual void openBookmarkURL( const QString& url, const QString& title )
                                { emit openURL( url, title ); }
    virtual QString currentURL() const;
    virtual QString currentTitle() const;

    KPopupMenu *menu() const { return m_menu; }

private slots:
    void slotBookmarksChanged( const QString &, const QString & caller );
    void slotEditBookmarks();

signals:
    void openURL( const QString& url, const QString& title );

private:
    void importOldBookmarks( const QString& path, KBookmarkManager* manager );

    Konsole *m_konsole;
    KPopupMenu *m_menu;
    KonsoleBookmarkMenu *m_bookmarkMenu;
    QString m_file;
};


#endif // KONSOLEBOOKMARKHANDLER_H
