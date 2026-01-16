/*
    SPDX-FileCopyrightText: 2018 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "DetachableTabBar.h"
#include "KonsoleSettings.h"
#include "widgets/ViewContainer.h"

#include <QApplication>
#include <QMimeData>
#include <QMouseEvent>

#include <KAcceleratorManager>

#include <QColor>
#include <QPainter>

namespace Konsole
{
DetachableTabBar::DetachableTabBar(QWidget *parent)
    : QTabBar(parent)
    , dragType(DragType::NONE)
    , _originalCursor(cursor())
    , tabId(-1)
    , _activityColor(QColor::Invalid)
{
    setAcceptDrops(true);
    setElideMode(Qt::TextElideMode::ElideLeft);
    KAcceleratorManager::setNoAccel(this);
}

void DetachableTabBar::setColor(int idx, const QColor &color)
{
    DetachableTabData data = tabData(idx).value<DetachableTabData>();
    if (data.color != color) {
        data.color = color;
        setDetachableTabData(idx, data);
        update(tabRect(idx));
    }
}

void DetachableTabBar::setActivityColor(int idx, const QColor &color)
{
    _activityColor = color;
    update();
}

void DetachableTabBar::removeColor(int idx)
{
    DetachableTabData data = tabData(idx).value<DetachableTabData>();
    if (data.color.isValid()) {
        data.color = QColor();
        setDetachableTabData(idx, data);
        update(tabRect(idx));
    }
}

void DetachableTabBar::setProgress(int idx, const std::optional<int> &progress)
{
    DetachableTabData data = tabData(idx).value<DetachableTabData>();
    if (data.progress != progress) {
        data.progress = progress;
        setDetachableTabData(idx, data);
        update(tabRect(idx));
    }
}

void DetachableTabBar::setDetachableTabData(int idx, const DetachableTabData &data)
{
    if ((data.color.isValid() && data.color.alpha() > 0) || data.progress.has_value()) {
        setTabData(idx, QVariant::fromValue(data));
    } else {
        setTabData(idx, QVariant());
    }
}

void DetachableTabBar::middleMouseButtonClickAt(const QPoint &pos)
{
    tabId = tabAt(pos);

    if (tabId != -1) {
        Q_EMIT closeTab(tabId);
    }
}

void DetachableTabBar::mousePressEvent(QMouseEvent *event)
{
    QTabBar::mousePressEvent(event);
    _containers = window()->findChildren<Konsole::TabbedViewContainer *>();
}

void DetachableTabBar::mouseMoveEvent(QMouseEvent *event)
{
    QTabBar::mouseMoveEvent(event);
    auto widgetAtPos = qApp->topLevelAt(event->globalPosition().toPoint());
    if (widgetAtPos != nullptr) {
        if (window() == widgetAtPos->window()) {
            if (dragType != DragType::NONE) {
                dragType = DragType::NONE;
                setCursor(_originalCursor);
            }
        } else {
            if (dragType != DragType::WINDOW) {
                dragType = DragType::WINDOW;
                setCursor(QCursor(Qt::DragMoveCursor));
            }
        }
    } else if (!contentsRect().adjusted(-30, -30, 30, 30).contains(event->pos())) {
        // Don't let it detach the last tab.
        if (count() == 1) {
            return;
        }
        if (dragType != DragType::OUTSIDE) {
            dragType = DragType::OUTSIDE;
            setCursor(QCursor(Qt::DragCopyCursor));
        }
    }
}

void DetachableTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    QTabBar::mouseReleaseEvent(event);

    switch (event->button()) {
    case Qt::MiddleButton:
        if (KonsoleSettings::closeTabOnMiddleMouseButton()) {
            middleMouseButtonClickAt(event->pos());
        }

        tabId = tabAt(event->pos());
        if (tabId == -1) {
            Q_EMIT newTabRequest();
        }
        break;
    case Qt::LeftButton:
        _containers = window()->findChildren<Konsole::TabbedViewContainer *>();
        break;
    default:
        break;
    }

    setCursor(_originalCursor);

    if (contentsRect().adjusted(-30, -30, 30, 30).contains(event->pos())) {
        return;
    }

    auto widgetAtPos = qApp->topLevelAt(event->globalPosition().toPoint());
    if (widgetAtPos == nullptr) {
        if (count() != 1) {
            Q_EMIT detachTab(currentIndex());
        }
    } else if (window() != widgetAtPos->window()) {
        if (_containers.size() == 1 || count() > 1) {
            Q_EMIT moveTabToWindow(currentIndex(), widgetAtPos);
        }
    }
}

void DetachableTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        QTabBar::mouseDoubleClickEvent(event);
    }
}

void DetachableTabBar::dragEnterEvent(QDragEnterEvent *event)
{
    const auto dragId = QStringLiteral("konsole/terminal_display");
    if (!event->mimeData()->hasFormat(dragId)) {
        return;
    }
    auto other_pid = event->mimeData()->data(dragId).toInt();
    // don't accept the drop if it's another instance of konsole
    if (qApp->applicationPid() != other_pid) {
        return;
    }
    event->accept();
}

void DetachableTabBar::dragMoveEvent(QDragMoveEvent *event)
{
    int tabIdx = tabAt(event->position().toPoint());
    if (tabIdx != -1) {
        setCurrentIndex(tabIdx);
    }
}

void DetachableTabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);
    if (!event->isAccepted()) {
        return; // Reduces repainting
    }

    QPainter painter(this);
    painter.setPen(Qt::NoPen);

    for (int tabIndex = 0; tabIndex < count(); tabIndex++) {
        const QVariant data = tabData(tabIndex);
        if (!data.isValid() || data.isNull()) {
            continue;
        }

        const DetachableTabData tabData = data.value<DetachableTabData>();

        const bool colorValid = tabData.color.isValid() && tabData.color.alpha() > 0;

        if (!colorValid && !tabData.progress.has_value()) {
            continue;
        }

        const QColor color = colorValid ? tabData.color : palette().highlight().color();

        painter.setBrush(color);
        QRect tRect = tabRect(tabIndex);
        tRect.setTop(painter.fontMetrics().height() + 6); // Color bar top position consider a height the font and fixed spacing of 6px
        tRect.setHeight(4);
        tRect.setLeft(tRect.left() + 6);
        tRect.setWidth(tRect.width() - 6);

        // Draw progress, if any, ontop of a faint bar.
        if (tabData.progress.has_value()) {
            painter.setOpacity(0.3);
            painter.drawRect(tRect);
            painter.setOpacity(1.0);

            tRect.setWidth(tRect.width() * tabData.progress.value() / 100.0);
            painter.drawRect(tRect);
        } else {
            painter.drawRect(tRect);
        }
    }
}

}

#include "moc_DetachableTabBar.cpp"
