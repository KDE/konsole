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

namespace Konsole
{
class KONSOLEPRIVATE_EXPORT ColorFilterHotSpot : public RegExpFilterHotSpot
{
public:
    ColorFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts, const QColor &color);
    ~ColorFilterHotSpot() override = default;

    QList<QAction *> actions() override;

    void mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev) override;
    void mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev) override;

private:
    void tooltipRequested();
    QMimeData *createMimeData() const;

    QColor _color;
    QPoint _toolPos = QPoint(0, 0);
    static bool _canGenerateTooltip;
};
}

#endif
