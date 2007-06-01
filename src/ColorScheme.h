/*
    This source file is part of Konsole, a terminal emulator.

    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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

#ifndef COLORSCHEME_H
#define COLORSCHEME_H

// Qt
#include <QtCore/QHash>
#include <QtCore/QList>
#include <QtCore/QMetaType>
#include <QtCore/QIODevice>
#include <QtCore/QSet>

// Konsole
#include "TECommon.h"

class QIODevice;
class KConfig;

namespace Konsole
{

/**
 * Represents a color scheme for a terminal display.  
 *
 * The color scheme includes the palette of colors used to draw the text and character backgrounds
 * in the display and the opacity level of the display background. 
 */
class ColorScheme
{
public:
    /** 
     * Constructs a new color scheme which is initialised to the default color set 
     * for Konsole.
     */
    ColorScheme();
    ColorScheme(const ColorScheme& other);
    ~ColorScheme();

    /** Sets the descriptive name of the color scheme. */
    void setDescription(const QString& description);
    /** Returns the descriptive name of the color scheme. */
    QString description() const;

    /** Sets the name of the color scheme */
    void setName(const QString& name);
    /** Returns the name of the color scheme */
    QString name() const;

    /** Reads the color scheme from the specified configuration source */
    void read(KConfig& config);
    /** Writes the color scheme to the specified configuration source */
    void write(KConfig& config) const;

    /** Sets a single entry within the color palette. */
    void setColorTableEntry(int index , const ColorEntry& entry);
    /** 
     * Returns a pointer to an array of color entries which form the palette
     * for this color scheme.  
     * The array has TABLE_COLORS entries
     *
     * TODO: More documentation
     */
    const ColorEntry* colorTable() const;

    /** 
     * Convenience method.  Returns the 
     * foreground color for this scheme, 
     * this is the primary color used to draw the 
     * text in this scheme.
     */
    QColor foregroundColor() const;
    /**
     * Convenience method.  Returns the background color for 
     * this scheme, this is the primary color used to 
     * draw the terminal background in this scheme.
     */
    QColor backgroundColor() const;

    /** 
     * Sets the opacity level of the display background.  A value of
     * 0 represents a completely transparent background, up to a value of 1
     * representing a completely opaque background.
     *
     * Defaults to 1.
     *
     * TODO: More documentation
     */
    void setOpacity(qreal opacity);
    /** 
     * Returns the opacity level for this color scheme, see setOpacity()
     * TODO: More documentation
     */
    qreal opacity() const;

    //void setCursorColor(const QColor& color);
    //QColor cursorColor() const;

    //void setCursorShape(int shape);
    //int cursorShape() const;

    static QString colorNameForIndex(int index);
private:
    // reads a single colour entry from a KConfig source
    ColorEntry readColorEntry(KConfig& config , const QString& colorName);
    // writes a single colour entry to a KConfig source
    void writeColorEntry(KConfig& config , const QString& colorName, const ColorEntry& entry) const;

    QString _description;
    QString _name;
    qreal _opacity;
    ColorEntry* _table; // pointer to custom color table or 0 if the default
                        // color scheme is being used

    static const char* colorNames[TABLE_COLORS];
    static const ColorEntry defaultTable[]; // table of default color entries
};

/**
 * Reads a color scheme stored in the .schema format used in the KDE 3 incarnation
 * of Konsole
 *
 * Only the basic essentials ( title and color palette entries ) are currently
 * supported.  Additional options such as background image and background
 * blend colors are ignored.
 */
class KDE3ColorSchemeReader
{
public:
    /** 
     * Constructs a new reader which reads from the specified device. 
     * The device should be open in read-only mode. 
     */
    KDE3ColorSchemeReader( QIODevice* device );

    /** 
     * Reads and parses the contents of the .schema file from the input
     * device and returns the ColorScheme defined within it.
     *
     * Returns a null pointer if an error occurs whilst parsing
     * the contents of the file.
     */
    ColorScheme* read();

private:
    // reads a line from the file specifying a colour palette entry
    // format is: color [index] [red] [green] [blue] [transparent] [bold]
    void readColorLine(const QString& line , ColorScheme* scheme);
    void readTitleLine(const QString& line , ColorScheme* scheme);

    QIODevice* _device;
};

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
     *
     * TODO - Ensure the old color scheme gets deleted
     */
    void addColorScheme(ColorScheme* scheme);

    /**
     * Deletes a color scheme.  
     */
    void deleteColorScheme(const QString& name);

    /** 
     * Returns a list of the all the available color schemes. 
     * This may be slow when first called because all of the color
     * scheme resources on disk must be located, read and parsed.
     *
     * Subsequent calls will be inexpensive. 
     */
    QList<const ColorScheme*> allColorSchemes();    

    /** Sets the global color scheme manager instance. */
    static void setInstance(ColorSchemeManager* instance);
    /** Returns the global color scheme manager instance. */
    static ColorSchemeManager* instance();

private:
    // loads a color scheme from a KDE 4+ .colorscheme file
    bool loadColorScheme(const QString& path);
    // loads a color scheme from a KDE 3 .schema file
    bool loadKDE3ColorScheme(const QString& path);
    // returns a list of paths of color schemes in the KDE 4+ .colorscheme file format
    QList<QString> listColorSchemes();
    // returns a list of paths of color schemes in the .schema file format
    // used in KDE 3
    QList<QString> listKDE3ColorSchemes();
    // loads all of the color schemes
    void loadAllColorSchemes();
    // finds the path of a color scheme
    QString findColorSchemePath(const QString& name) const;

    QHash<QString,const ColorScheme*> _colorSchemes;
    QSet<ColorScheme*> _modifiedSchemes;

    bool _haveLoadedAll;

    static const ColorScheme _defaultColorScheme;
    static ColorSchemeManager* _instance;
};

}

Q_DECLARE_METATYPE(const Konsole::ColorScheme*)

#endif //COLORSCHEME_H
