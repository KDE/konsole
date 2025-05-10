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
#include <QImage>
#include <QImageReader>
#include <QMovie>
#include <QPainter>

using namespace Konsole;

ColorSchemeWallpaper::ColorSchemeWallpaper(const QString &path,
                                           const ColorSchemeWallpaper::FillStyle style,
                                           const QPointF &anchor,
                                           const qreal &opacity,
                                           const ColorSchemeWallpaper::FlipType flipType)
    : _path(path)
    , _picture(nullptr)
    , _movie(nullptr)
    , _style(style)
    , _anchor(anchor)
    , _opacity(opacity)
    , _flipType(flipType)
    , _isAnimated(false)
    , _frameDelay(17) // Approx. 60FPS
{
    float x = _anchor.x(), y = _anchor.y();

    if (x < 0 || x > 1.0f || y < 0 || y > 1.0f)
        _anchor = QPointF(0.5, 0.5);
}

ColorSchemeWallpaper::~ColorSchemeWallpaper() = default;

void ColorSchemeWallpaper::load()
{
    if (_path.isEmpty()) {
        return;
    }

    QImageReader reader(_path);
    _isAnimated = reader.supportsAnimation();
    if (_isAnimated) {
        if (_movie == nullptr) {
            _movie = std::make_unique<QMovie>();
        }

        if (!_movie->isValid()) {
            _movie->setFileName(_path);
            _movie->start();
        }

        // Initialize _picture in load, to avoid null checking for both _picture and _movie in draw.
        if (_picture == nullptr) {
            _picture = std::make_unique<QPixmap>();
        }

        if (_picture->isNull()) {
            const QImage image = _movie->currentImage();
            const QImage transformed = FlipImage(image, _flipType);
            _picture->convertFromImage(transformed);
        }
    } else {
        // Cleanup animated image
        if (_movie != nullptr) {
            if (_movie->isValid()) {
                _movie->stop();
            }
            _movie.reset();
        }

        // Create and load original pixmap
        if (_picture == nullptr) {
            _picture = std::make_unique<QPixmap>();
        }

        if (_picture->isNull()) {
            const QImage image(_path);
            const QImage transformed = FlipImage(image, _flipType);
            _picture->convertFromImage(transformed);
        }
    }
}

bool ColorSchemeWallpaper::isNull() const
{
    return _path.isEmpty();
}

bool ColorSchemeWallpaper::draw(QPainter &painter, const QRect& rect, qreal bgColorOpacity, const QColor &backgroundColor)
{
    if ((_picture == nullptr) || _picture->isNull()) {
        return false;
    }

    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(bgColorOpacity);
    painter.fillRect(rect, backgroundColor);
    painter.setOpacity(_opacity);

    if (_isAnimated) {
        if (_movie->state() == QMovie::NotRunning) {
            _movie->start();
        }
        if (_picture != nullptr) {
            _picture.reset();
        }
        if (_picture == nullptr) {
            _picture = std::make_unique<QPixmap>();
        }
        if (_picture->isNull()) {
            QImage currentImage = _movie->currentImage();
            const QImage transformed = FlipImage(currentImage, _flipType);
            _picture->convertFromImage(transformed);
        }
    }
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

ColorSchemeWallpaper::FlipType ColorSchemeWallpaper::flipType() const
{
    return _flipType;
}

QPointF ColorSchemeWallpaper::anchor() const
{
    return _anchor;
}

qreal ColorSchemeWallpaper::opacity() const
{
    return _opacity;
}

QRectF ColorSchemeWallpaper::ScaledRect(const QSize &viewportSize, const QSize &pictureSize, const QRect &rect)
{
    QRectF scaledRect = QRectF();
    QSize scaledSize = _style == NoScaling ? pictureSize : pictureSize.scaled(viewportSize, RatioMode());

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

QImage ColorSchemeWallpaper::FlipImage(const QImage &image, const ColorSchemeWallpaper::FlipType flipType)
{
    switch (flipType) {
    case Horizontal:
        return image.mirrored(true, false);
    case Vertical:
        return image.mirrored(false, true);
    case Both:
        return image.mirrored(true, true);
    default:
        return image;
    }
}

bool ColorSchemeWallpaper::isAnimated() const
{
    return _isAnimated;
}

int ColorSchemeWallpaper::getFrameDelay() const
{
    return _frameDelay;
}

#include "moc_ColorSchemeWallpaper.cpp"
