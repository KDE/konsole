#include "DetachableTabBar.h"
#include <QMouseEvent>

DetachableTabBar::DetachableTabBar(QWidget *parent) :
    QTabBar(parent),
    _draggingOutside(false) {}

void DetachableTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
    if (!contentsRect().adjusted(-30,-30,30,30).contains(event->pos())) {
        if (!_draggingOutside) {
            _draggingOutside = true;
            _originalCursor = cursor();
            setCursor(QCursor(Qt::DragCopyCursor));
        }
    } else if (_draggingOutside) {
        _draggingOutside = false;
        setCursor(_originalCursor);
    }
}

void DetachableTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent(event);
    if (!contentsRect().adjusted(-30,-30,30,30).contains(event->pos())) {
        setCursor(_originalCursor);
        emit detachTab(currentIndex());
    }
}
