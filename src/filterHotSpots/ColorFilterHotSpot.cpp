/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ColorFilterHotSpot.h"

#include <QApplication>
#include <QBuffer>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QToolTip>

#include "KonsoleSettings.h"

using namespace Konsole;

bool ColorFilterHotSpot::_canGenerateTooltip = false;

ColorFilterHotSpot::ColorFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts, const QColor &color)
    : RegExpFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts)
    , _color(color)
{
    setType(Color);
}

void ColorFilterHotSpot::mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    HotSpot::mouseEnterEvent(td, ev);
    _toolPos = ev->globalPos();
    _canGenerateTooltip = true;

    QTimer::singleShot(100, this, [&] {
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

    Q_ASSERT(_color.isValid());

    int sideUnit = 10;
    int sideLength = sideUnit * sideUnit;
    QRect square(0, 0, sideLength, sideLength);

    QPixmap pix(square.size());
    QPainter paint(&pix);

    // Add a nice checkerboard pattern for translucent colors.
    if (_color.alpha() < 255) {
        // Base background
        paint.fillRect(square, Qt::lightGray);

        // Checkerboard pattern
        QBrush brush(Qt::white, Qt::Dense4Pattern);
        brush.setTransform(QTransform::fromScale(sideUnit, sideUnit));
        paint.setBrush(brush);
        paint.drawRect(square);
    }

    paint.fillRect(square, _color);

    QByteArray data;
    QBuffer buffer(&data);
    pix.save(&buffer, "PNG");

    const auto tooltipString = QStringLiteral("<img src='data:image/png;base64, %0'>").arg(QString::fromLocal8Bit(data.toBase64()));

    QPoint tooltipPosition = QPoint(_toolPos.x(), _toolPos.y());
    QToolTip::showText(tooltipPosition, tooltipString, qApp->focusWidget());
}
