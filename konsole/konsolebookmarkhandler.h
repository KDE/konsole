// Born as kdelibs/kio/kfile/kfilebookmarkhandler.h

#ifndef KONSOLEBOOKMARKHANDLER_H
#define KONSOLEBOOKMARKHANDLER_H

#include <kbookmarkmanager.h>
#include <konsolebookmarkmenu.h>

class QTextStream;
class KPopupMenu;
class KonsoleBookmarkMenu;

class KonsoleBookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    KonsoleBookmarkHandler( Konsole *konsole, bool toplevel );

    QPopupMenu * popupMenu();

    // KBookmarkOwner interface:
    virtual void openBookmarkURL( const QString& url, const QString& title )
                                { emit openURL( url, title ); }
    virtual QString currentURL() const;
    virtual QString currentTitle() const;

    KPopupMenu *menu() const { return m_menu; }

signals:
    void openURL( const QString& url, const QString& title );

private slots:
    // for importing
    void slotNewBookmark( const QString& text, const QCString& url,
                          const QString& additionalInfo );
    void slotNewFolder( const QString& text, bool open,
                        const QString& additionalInfo );
    void slotBookmarksChanged( const QString &, const QString & caller );
    void newSeparator();
    void endFolder();

private:
    void importOldBookmarks( const QString& path, const QString& destinationPath );

    Konsole *m_konsole;
    KPopupMenu *m_menu;
    KonsoleBookmarkMenu *m_bookmarkMenu;
    
    QTextStream *m_importStream;

protected:
    virtual void virtual_hook( int id, void* data );
private:
    class KonsoleBookmarkHandlerPrivate;
    KonsoleBookmarkHandlerPrivate *d;
};


#endif // KONSOLEBOOKMARKHANDLER_H
