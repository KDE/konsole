/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COLORSCHEMEWALLPAPER_H
#define COLORSCHEMEWALLPAPER_H
// STD
#include <memory>

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
    std::unique_ptr<QPixmap> _picture;
};

}

#endif
