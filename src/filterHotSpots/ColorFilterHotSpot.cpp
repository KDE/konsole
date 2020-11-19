/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ColorFilterHotSpot.h"

#include <QApplication>
#include <QTimer>
#include <QBuffer>
#include <QToolTip>
#include <QMouseEvent>
#include <QPixmap>
#include <QPainter>

#include "KonsoleSettings.h"

using namespace Konsole;

bool ColorFilterHotSpot::_canGenerateTooltip = false;

ColorFilterHotSpot::ColorFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts, const QString &colorName)
    : RegExpFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts)
    , _colorName(colorName)
{
    setType(Color);
}

void ColorFilterHotSpot::mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    HotSpot::mouseEnterEvent(td, ev);
    _toolPos = ev->globalPos();
    _canGenerateTooltip = true;

    QTimer::singleShot(100, this, [&]{
        tooltipRequested();
    });
}

void ColorFilterHotSpot::mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev) 
{
    HotSpot::mouseLeaveEvent(td, ev);
    _canGenerateTooltip = false;
    QToolTip::hideText();
}

void ColorFilterHotSpot::tooltipRequested()
{
    if (!_canGenerateTooltip) {
        return;
    }
    
    QColor toolColor(_colorName);
    QPoint tooltipPosition = QPoint(_toolPos.x(), _toolPos.y());

    QPixmap pix(100, 100);
    QPainter paint(&pix);
    paint.setPen(toolColor);
    paint.drawRect(0, 0, 100, 100);
    paint.fillRect(0, 0, 100, 100, toolColor);

    QByteArray data;
    QBuffer buffer(&data);
    pix.save(&buffer, "PNG");
    
    const auto tooltipString = QStringLiteral("<img src='data:image/png;base64, %0'>")
        .arg(QString::fromLocal8Bit(data.toBase64()));
    QToolTip::showText(tooltipPosition, tooltipString, qApp->focusWidget());
}
