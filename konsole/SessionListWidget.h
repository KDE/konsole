#ifndef SESSIONLISTWIDGET_H
#define SESSIONLISTWIDGET_H

// Qt
#include <QListWidget>

class SessionListWidget : public QListWidget
{
Q_OBJECT

public:
    SessionListWidget(QWidget* parent);

signals:
    void takeSessionEvent(int itemIndex);
    void dropSessionEvent(int id); 

protected:
    virtual void startDrag(Qt::DropActions supportedActions);
    virtual void dropEvent(QDropEvent* event);
    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void dragMoveEvent(QDragMoveEvent* event);

};

/*class SessionListDelegate : public QAbstractItemDelegate
{
Q_OBJECT

public:
    virtual void paint(QPainter* painter , const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;

    virtual QSize sizeHint( const QStyleOptionViewItem& option,
                            const QModelIndex& index) const;
};*/

#endif // SESSIONLISTWIDGET_H
