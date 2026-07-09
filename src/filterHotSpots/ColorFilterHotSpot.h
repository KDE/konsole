/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COLORFILTERHOTSPOT_H
#define COLORFILTERHOTSPOT_H

#include "RegExpFilterHotspot.h"

#include <QColor>
#include <QPoint>

class QMimeData;
class QPixmap;

namespace Konsole
{
class KONSOLEPRIVATE_EXPORT ColorFilterHotSpot : public RegExpFilterHotSpot
{
public:
    ColorFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts, const QColor &color);
    ~ColorFilterHotSpot() override = default;

    QList<QAction *> actions() override;

    bool hasDragOperation() const override;
    void startDrag() override;

    void mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev) override;
    void mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev) override;

private:
    void tooltipRequested();
    QMimeData *createMimeData() const;
    enum class PixmapOption {
        NoFrame,
        DrawFrame
    };
    QPixmap createPixmap(int extent, int checkBoardUnitExtent, PixmapOption pixmapOption) const;

    QColor _color;
    QPoint _toolPos = QPoint(0, 0);
    static bool _canGenerateTooltip;
};
}

#endif
