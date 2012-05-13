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

#ifndef COLORSCHEMEMANAGER_H
#define COLORSCHEMEMANAGER_H

// Qt
#include <QtCore/QHash>
#include <QtCore/QStringList>

// Konsole
#include "ColorScheme.h"

namespace Konsole
{
/**
 * Manages the color schemes available for use by terminal displays.
 * See ColorScheme
 */
class ColorSchemeManager
{
public:
    /**
     * Constructs a new ColorSchemeManager and loads the list
     * of available color schemes.
     *
     * The color schemes themselves are not loaded until they are first
     * requested via a call to findColorScheme()
     */
    ColorSchemeManager();
    /**
     * Destroys the ColorSchemeManager and saves any modified color schemes to disk.
     */
    ~ColorSchemeManager();

    /**
     * Returns the default color scheme for Konsole
     */
    const ColorScheme* defaultColorScheme() const;

    /**
     * Returns the color scheme with the given name or 0 if no
     * scheme with that name exists.  If @p name is empty, the
     * default color scheme is returned.
     *
     * The first time that a color scheme with a particular name is
     * requested, the configuration information is loaded from disk.
     */
    const ColorScheme* findColorScheme(const QString& name);

    /**
     * Adds a new color scheme to the manager.  If @p scheme has the same name as
     * an existing color scheme, it replaces the existing scheme.
     */
    void addColorScheme(ColorScheme* scheme);

    /**
     * Deletes a color scheme.  Returns true on successful deletion or false otherwise.
     */
    bool deleteColorScheme(const QString& name);

    /**
     * Returns a list of the all the available color schemes.
     * This may be slow when first called because all of the color
     * scheme resources on disk must be located, read and parsed.
     *
     * Subsequent calls will be inexpensive.
     */
    QList<const ColorScheme*> allColorSchemes();

    /** Returns the global color scheme manager instance. */
    static ColorSchemeManager* instance();

private:
    // loads a color scheme from a KDE 4+ .colorscheme file
    bool loadColorScheme(const QString& path);
    // loads a color scheme from a KDE 3 .schema file
    bool loadKDE3ColorScheme(const QString& path);
    // returns a list of paths of color schemes in the KDE 4+ .colorscheme file format
    QStringList listColorSchemes();
    // returns a list of paths of color schemes in the .schema file format
    // used in KDE 3
    QStringList listKDE3ColorSchemes();
    // loads all of the color schemes
    void loadAllColorSchemes();
    // finds the path of a color scheme
    QString findColorSchemePath(const QString& name) const;

    QHash<QString, const ColorScheme*> _colorSchemes;

    bool _haveLoadedAll;

    static const ColorScheme _defaultColorScheme;
};
}

#endif //COLORSCHEMEMANAGER_H
