/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ColorSchemeWallpaper.h"

// Konsole
#include "ColorScheme.h"

// Qt
#include <QPainter>

using namespace Konsole;

ColorSchemeWallpaper::ColorSchemeWallpaper(const QString &path, const ColorSchemeWallpaper::FillStyle style, const QPointF &anchor)
    : _path(path)
    , _picture(nullptr)
    , _style(style)
    , _anchor(anchor)
{
    float x = _anchor.x(), y = _anchor.y();
    
    if(x < 0 || x > 1.0f || y < 0 || y > 1.0f)
        _anchor = QPointF(0.5f, 0.5f);
}

ColorSchemeWallpaper::~ColorSchemeWallpaper() = default;

void ColorSchemeWallpaper::load()
{
    if (_path.isEmpty()) {
        return;
    }

    // Create and load original pixmap
    if (_picture == nullptr) {
        _picture = std::make_unique<QPixmap>();
    }

    if (_picture->isNull()) {
        _picture->load(_path);
    }
}

bool ColorSchemeWallpaper::isNull() const
{
    return _path.isEmpty();
}

bool ColorSchemeWallpaper::draw(QPainter &painter, const QRect rect, qreal opacity)
{
    if ((_picture == nullptr) || _picture->isNull()) {
        return false;
    }

    if (qFuzzyCompare(qreal(1.0), opacity)) {
        if (_style == Tile) {
            painter.drawTiledPixmap(rect, *_picture, rect.topLeft());
        } else {
            QRectF srcRect = ScaledRect(painter.viewport().size(), _picture->size(), rect);
            painter.drawPixmap(rect, *_picture, srcRect);
        }
        return true;
    }

    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, QColor(0, 0, 0, 0));
    painter.setOpacity(opacity);

    if (_style == Tile) {
        painter.drawTiledPixmap(rect, *_picture, rect.topLeft());
    } else {
        QRectF srcRect = ScaledRect(painter.viewport().size(), _picture->size(), rect);
        painter.drawPixmap(rect, *_picture, srcRect);
    }

    painter.restore();

    return true;
}

QString ColorSchemeWallpaper::path() const
{
    return _path;
}

ColorSchemeWallpaper::FillStyle ColorSchemeWallpaper::style() const
{
    return _style;
}

QPointF ColorSchemeWallpaper::anchor() const
{
    return _anchor;
} 

QRectF ColorSchemeWallpaper::ScaledRect(const QSize viewportSize, const QSize pictureSize, const QRect rect)
{
    QRectF scaledRect = QRectF();
    QSize scaledSize = _style == NoResize ? pictureSize : pictureSize.scaled(viewportSize, RatioMode());

    double scaleX = pictureSize.width() / static_cast<double>(scaledSize.width());
    double scaleY = pictureSize.height() / static_cast<double>(scaledSize.height());

    double offsetX = (scaledSize.width() - viewportSize.width()) * _anchor.x();
    double offsetY = (scaledSize.height() - viewportSize.height()) * _anchor.y();

    scaledRect.setX((rect.x() + offsetX) * scaleX);
    scaledRect.setY((rect.y() + offsetY) * scaleY);

    scaledRect.setWidth(rect.width() * scaleX);
    scaledRect.setHeight(rect.height() * scaleY);

    return scaledRect;
}

Qt::AspectRatioMode ColorSchemeWallpaper::RatioMode()
{
    switch (_style) {
    case Crop:
        return Qt::KeepAspectRatioByExpanding;
    case Adapt:
        return Qt::KeepAspectRatio;
    case Tile:
    case Stretch:
    default:
        return Qt::IgnoreAspectRatio;
    }
}
