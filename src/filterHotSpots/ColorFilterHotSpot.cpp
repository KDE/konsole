/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ColorFilterHotSpot.h"

#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDrag>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QTimer>
#include <QToolTip>

#include <KLocalizedString>

#include "KonsoleSettings.h"

using namespace Konsole;

bool ColorFilterHotSpot::_canGenerateTooltip = false;

ColorFilterHotSpot::ColorFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts, const QColor &color)
    : RegExpFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts)
    , _color(color)
{
    setType(Color);
}

QMimeData *ColorFilterHotSpot::createMimeData() const
{
    auto *mimeData = new QMimeData();
    mimeData->setColorData(_color);
    mimeData->setText(_color.name());
    return mimeData;
}

QPixmap ColorFilterHotSpot::createPixmap(int extent, int checkBoardUnitExtent, PixmapOption pixmapOption) const
{
    const QRect square(0, 0, extent, extent);

    QPixmap pix(square.size());
    QPainter paint(&pix);

    // Add a nice checkerboard pattern for translucent colors.
    if (_color.alpha() < 255) {
        paint.save();

        // Checkerboard pattern
        static const char *const checkerboardXpm[] = {
            "2 2 2 1", // 2x2, 2 colors, 1 char/pixel
            ". c #FFFFFF", // Qt::white
            "X c #C0C0C0", // Qt::lightGray
            // pixels
            "X.",
            ".X",
        };
        const QPixmap texture(checkerboardXpm);

        QBrush brush(texture);
        brush.setTransform(QTransform::fromScale(checkBoardUnitExtent, checkBoardUnitExtent));
        paint.setBrush(brush);
        paint.setPen(Qt::NoPen);
        paint.drawRect(square);

        paint.restore();
    }

    paint.fillRect(square, _color);

    if (pixmapOption == PixmapOption::DrawFrame) {
        paint.drawRect(0, 0, extent - 1, extent - 1);
    }

    return pix;
}

QList<QAction *> ColorFilterHotSpot::actions()
{
    auto *action = new QAction(i18nc("@action:inmenu", "Copy Color"), this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    connect(action, &QAction::triggered, this, [this] {
        auto *mimeData = createMimeData();
        QApplication::clipboard()->setMimeData(mimeData);
    });
    return {action};
}

bool ColorFilterHotSpot::hasDragOperation() const
{
    return true;
}

void ColorFilterHotSpot::startDrag(TerminalDisplay *, QPoint)
{
    auto *drag = new QDrag(this);

    auto *mimeData = createMimeData();
    drag->setMimeData(mimeData);

    const QPixmap preview = createPixmap(24, 12, PixmapOption::DrawFrame);
    drag->setPixmap(preview);

    drag->exec(Qt::CopyAction);
}

void ColorFilterHotSpot::mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    HotSpot::mouseEnterEvent(td, ev);
    _toolPos = ev->globalPosition().toPoint();
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

    const QPixmap pix = createPixmap(sideLength, sideUnit, PixmapOption::NoFrame);

    QByteArray data;
    QBuffer buffer(&data);
    pix.save(&buffer, "PNG");

    const auto tooltipString = QStringLiteral("<img src='data:image/png;base64, %0'/><p><a>%1</a>")
                                   .arg(QString::fromLocal8Bit(data.toBase64()), i18nc("Caption of hover tooltip", "Color Preview"));

    QPoint tooltipPosition = QPoint(_toolPos.x(), _toolPos.y());
    QToolTip::showText(tooltipPosition, tooltipString, qApp->focusWidget());
}
