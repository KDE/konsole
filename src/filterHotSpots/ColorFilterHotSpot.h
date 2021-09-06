/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COLORFILTERHOTSPOT_H
#define COLORFILTERHOTSPOT_H

#include "RegExpFilterHotspot.h"

#include <QColor>
#include <QPoint>
#include <QPointer>

namespace Konsole
{
class ColorFilterHotSpot : public RegExpFilterHotSpot
{
public:
    ColorFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts, const QColor &color);
    ~ColorFilterHotSpot() override = default;

    void mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev) override;
    void mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev) override;

private:
    void tooltipRequested();

    QColor _color;
    QPoint _toolPos;
    static bool _canGenerateTooltip;
};
}

#endif
