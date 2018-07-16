#include "DetachableTabBar.h"
#include <QMouseEvent>

DetachableTabBar::DetachableTabBar(QWidget *parent) : QTabBar(parent){}

void DetachableTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
}

void DetachableTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent(event);
    if (!contentsRect().contains(event->pos())) {
        emit detachTab(currentIndex());
    }
}
