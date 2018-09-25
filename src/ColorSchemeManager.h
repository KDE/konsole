/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2018 by Harald Sitter <sitter@kde.org>

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
#include <QHash>
#include <QStringList>

// Konsole
#include "ColorScheme.h"

namespace Konsole {
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
    const ColorScheme *defaultColorScheme() const;

    /**
     * Returns the color scheme with the given name or 0 if no
     * scheme with that name exists.  If @p name is empty, the
     * default color scheme is returned.
     *
     * The first time that a color scheme with a particular name is
     * requested, the configuration information is loaded from disk.
     */
    const ColorScheme *findColorScheme(const QString &name);

    /**
     * Adds a new color scheme to the manager.  If @p scheme has the same name as
     * an existing color scheme, it replaces the existing scheme.
     */
    void addColorScheme(ColorScheme *scheme);

    /**
     * Deletes a color scheme.  Returns true on successful deletion or false otherwise.
     */
    bool deleteColorScheme(const QString &name);

    /**
     * Returns a list of the all the available color schemes.
     * This may be slow when first called because all of the color
     * scheme resources on disk must be located, read and parsed.
     *
     * Subsequent calls will be inexpensive.
     */
    QList<const ColorScheme *> allColorSchemes();

    /** Returns the global color scheme manager instance. */
    static ColorSchemeManager *instance();

    /**
     * Returns @c true if a colorscheme with @p name exists both under
     * the user's home dir location, and a system-wide location
     */
    bool canResetColorScheme(const QString &name);

    /**
     * Returns @c true if a colorscheme with @p name exists under the
     * user's home dir location, and hence can be deleted
     */
    bool isColorSchemeDeletable(const QString &name);

    // loads a color scheme from a KDE 4+ .colorscheme file
    bool loadColorScheme(const QString &filePath);

    // unloads a color scheme by it's file path (doesn't delete!)
    bool unloadColorScheme(const QString &filePath);

    // @returns the scheme name of a given file or a null string if the file is
    //   no theme
    static QString colorSchemeNameFromPath(const QString &path);

private:
    // returns a list of paths of color schemes in the KDE 4+ .colorscheme file format
    QStringList listColorSchemes();
    // loads all of the color schemes
    void loadAllColorSchemes();
    // finds the path of a color scheme
    QString findColorSchemePath(const QString &name) const;
    // @returns whether a path is a valid color scheme name
    static bool pathIsColorScheme(const QString &path);

    QHash<QString, const ColorScheme *> _colorSchemes;

    bool _haveLoadedAll;

    static const ColorScheme _defaultColorScheme;

    Q_DISABLE_COPY(ColorSchemeManager)
};
}

#endif //COLORSCHEMEMANAGER_H
