#ifndef NAVIGATIONITEM_H
#define NAVIGATIONITEM_H

// Qt
#include <QIcon>
#include <QList>
#include <QString>

class QAction;

//Terminal Session Navigation
class TESession;

/**
 * A class which provides information about particular views in a ViewContainer
 * for use in the ViewContainer's navigation widget.
 * 
 * This includes a title and an icon associated with a particular view,
 * as well as a list of actions which can should be shown when the entry
 * for that view is right-clicked in the navigation widget.
 *
 * NavigationItem instances also provide signals which are emitted 
 * when the title or icon associated with the view changes.
 */
class NavigationItem : public QObject
{
Q_OBJECT
    
public:
    NavigationItem();

    QString title() const;
    QIcon icon() const;
    virtual QList<QAction*> contextMenuActions( QList<QAction*> viewActions ) ;

signals:
    void titleChanged( NavigationItem* item );
    void iconChanged( NavigationItem* item );
    
protected:
    void setTitle( const QString& title );
    void setIcon( const QIcon& icon );
    
    
private:
    QString _title;
    QIcon _icon;
};

/**
 * A navigation item class which provides information about views
 * of terminal sessions.
 */
class SessionNavigationItem : public NavigationItem
{
    Q_OBJECT

public:
    /** 
     * Constructs a new SessionNavigationItem.
     *
     * @param session The terminal session from which the title, icon
     * and other information should be obtained. 
     */
    SessionNavigationItem(TESession* session);

    virtual QList<QAction*> contextMenuActions( QList<QAction*> viewActions ) ;

private slots:
    void updateTitle();

    //context menu action receivers
    void renameSession();

private:
    TESession* _session;
};


#endif //NAVIGATIONITEM_H
