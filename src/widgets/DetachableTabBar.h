/*
    SPDX-FileCopyrightText: 2018 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef DETACHABLETABBAR_H
#define DETACHABLETABBAR_H

#include <QCursor>
#include <QTabBar>

#include <optional>

class QColor;
class QPaintEvent;

namespace Konsole
{
class TabbedViewContainer;

struct DetachableTabData {
    QColor color;
    std::optional<int> progress;
};

class DetachableTabBar : public QTabBar
{
    Q_OBJECT
public:
    enum class DragType : unsigned char {
        NONE,
        OUTSIDE,
        WINDOW,
    };

    explicit DetachableTabBar(QWidget *parent = nullptr);

    void setColor(int idx, const QColor &color);
    void removeColor(int idx);

    void setProgress(int idx, int progress);
    void removeProgress(int idx);

Q_SIGNALS:
    void detachTab(int index);
    void moveTabToWindow(int tabIndex, QWidget *otherWindow);
    void closeTab(int index);
    void newTabRequest();

protected:
    void middleMouseButtonClickAt(const QPoint &pos);
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setDetachableTabData(int idx, const DetachableTabData &data);

    DragType dragType;
    QCursor _originalCursor;
    QList<TabbedViewContainer *> _containers;
    int tabId;
};
}

Q_DECLARE_METATYPE(Konsole::DetachableTabData)

#endif
