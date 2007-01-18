
// Qt
#include <QtDebug>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

// Konsole
#include "SessionListWidget.h"

static const char* konsoleSessionMimeFormat = "konsole/session";

SessionListWidget::SessionListWidget(QWidget* parent)
    : QListWidget(parent)
{
    // use large icons so that there is a big area for the user to click
    // on to switch between sessions
    setIconSize( QSize(32,32) );

    // turn the frame off
    setFrameStyle( QFrame::NoFrame );

    QPalette p = palette();
    p.setBrush( QPalette::Base , QColor(220,220,220) );
    setPalette(p);
}

void SessionListWidget::startDrag(Qt::DropActions /*supportedActions*/)
{
    qDebug() << "drag and drop started in session list widget";

    QMimeData* mimeData = new QMimeData();

    QByteArray data;
    data.setNum(42);
    mimeData->setData(konsoleSessionMimeFormat,data);

    QDrag* drag = new QDrag(this);
    drag->setMimeData(mimeData);

    Qt::DropAction action = drag->start( Qt::MoveAction );

    if ( action & Qt::MoveAction )
    {
        emit takeSessionEvent(currentRow());
    }
}

void SessionListWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if ( event->mimeData()->hasFormat(konsoleSessionMimeFormat) )
    {
        event->accept();
    }
}

void SessionListWidget::dragMoveEvent(QDragMoveEvent* event)
{
    if ( event->mimeData()->hasFormat(konsoleSessionMimeFormat) )
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    }
}

void SessionListWidget::dropEvent(QDropEvent* event)
{
    if ( event->mimeData()->hasFormat(konsoleSessionMimeFormat) )
    {
        event->setDropAction(Qt::MoveAction);
        event->accept();

        emit dropSessionEvent( event->mimeData()->data(konsoleSessionMimeFormat).toInt() ); 
    }
}


#include "SessionListWidget.moc"
