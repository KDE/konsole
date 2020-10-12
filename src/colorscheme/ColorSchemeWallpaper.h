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

#ifndef COLORSCHEMEWALLPAPER_H
#define COLORSCHEMEWALLPAPER_H

// Qt
#include <QMetaType>
#include <QSharedData>

// Konsole
#include "../characters/CharacterColor.h"

class QPixmap;
class QPainter;

namespace Konsole
{
    /**
     * This class holds the wallpaper pixmap associated with a color scheme.
     * The wallpaper object is shared between multiple TerminalDisplay.
     */
    class ColorSchemeWallpaper : public QSharedData
    {
    public:
        typedef QExplicitlySharedDataPointer<ColorSchemeWallpaper> Ptr;

        explicit ColorSchemeWallpaper(const QString &path);
        ~ColorSchemeWallpaper();

        void load();

        /** Returns true if wallpaper available and drawn */
        bool draw(QPainter &painter, const QRect rect, qreal opacity = 1.0);

        bool isNull() const;

        QString path() const;

    private:
        Q_DISABLE_COPY(ColorSchemeWallpaper)

        QString _path;
        QPixmap *_picture;
    };

}

#endif
