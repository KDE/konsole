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
    if (!contentsRect().adjusted(-10,-10,10,10).contains(event->pos())) {
        emit detachTab(currentIndex());
    }
}
