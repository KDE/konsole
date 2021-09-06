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

ColorSchemeWallpaper::ColorSchemeWallpaper(const QString &path)
    : _path(path)
    , _picture(nullptr)
{
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
        painter.drawTiledPixmap(rect, *_picture, rect.topLeft());
        return true;
    }

    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, QColor(0, 0, 0, 0));
    painter.setOpacity(opacity);
    painter.drawTiledPixmap(rect, *_picture, rect.topLeft());
    painter.restore();
    return true;
}

QString ColorSchemeWallpaper::path() const
{
    return _path;
}
