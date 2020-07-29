/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "ColorSchemeWallpaper.h"

// Konsole
#include "ColorScheme.h"

// Qt
#include <QPainter>

using namespace Konsole;

ColorSchemeWallpaper::Ptr ColorScheme::wallpaper() const
{
    return _wallpaper;
}

ColorSchemeWallpaper::ColorSchemeWallpaper(const QString &path) :
    _path(path),
    _picture(nullptr)
{
}

ColorSchemeWallpaper::~ColorSchemeWallpaper()
{
    delete _picture;
}

void ColorSchemeWallpaper::load()
{
    if (_path.isEmpty()) {
        return;
    }

    // Create and load original pixmap
    if (_picture == nullptr) {
        _picture = new QPixmap();
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
