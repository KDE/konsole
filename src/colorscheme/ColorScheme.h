/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COLORSCHEME_H
#define COLORSCHEME_H

// Qt
#include <QMetaType>
#include <QSharedData>

// C++
#include <memory>

// Konsole
#include "ColorSchemeWallpaper.h"

#include <ki18n_version.h>
#if KI18N_VERSION >= QT_VERSION_CHECK(5, 89, 0)
#include <KLazyLocalizedString>
#endif

class KConfig;
class QPixmap;
class QPainter;

namespace Konsole
{
class RandomizationRange;
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
     * Constructs a new color scheme which is initialized to the default color set
     * for Konsole.
     */
    ColorScheme();
    ColorScheme(const ColorScheme &other);
    ColorScheme &operator=(const ColorScheme &other) = delete;
    ~ColorScheme();

    /** Sets the descriptive name of the color scheme. */
    void setDescription(const QString &description);
    /** Returns the descriptive name of the color scheme. */
    QString description() const;

    /** Sets the name of the color scheme */
    void setName(const QString &name);
    /** Returns the name of the color scheme */
    QString name() const;

    /** Reads the color scheme from the specified configuration source */
    void read(const KConfig &config);
    /** Writes the color scheme to the specified configuration source */
    void write(KConfig &config) const;

    /** Sets a single entry within the color palette. */
    void setColorTableEntry(int index, const QColor &entry);

    /**
     * Copies the color entries which form the palette for this color scheme
     * into @p table.  @p table should be an array with TABLE_COLORS entries.
     *
     * @param table Array into which the color entries for this color scheme
     * are copied.
     * @param randomSeed Color schemes may allow certain colors in their
     * palette to be randomized.  The seed is used to pick the random color.
     */
    void getColorTable(QColor *table, uint randomSeed = 0) const;

    /**
     * Retrieves a single color entry from the table.
     *
     * See getColorTable()
     */
    QColor colorEntry(int index, uint randomSeed = 0) const;

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
     * Returns true if this color scheme has a dark background.
     * The background color is said to be dark if it has a lightness
     * of less than 50% in the HSLuv color space.
     */
    bool hasDarkBackground() const;

    /**
     * Sets the opacity level of the display background. @p opacity ranges
     * between 0 (completely transparent background) and 1 (completely
     * opaque background).
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

    /**
     * Enables blur behind the transparent window
     *
     * Defaults to false.
     */
    void setBlur(bool blur);
    /**
     * Returns whether blur is enabled for this color scheme, see setBlur()
     */
    bool blur() const;

    void setWallpaper(const QString &path, const ColorSchemeWallpaper::FillStyle style, const QPointF &anchor, const qreal &opacity);
    void setWallpaper(const QString &path, const QString &style, const QPointF &anchor, const qreal &opacity);

    ColorSchemeWallpaper::Ptr wallpaper() const;

    /**
     * Enables colors randomization. This will cause the palette
     * returned by getColorTable() and colorEntry() to be adjusted
     * depending on the parameters of color randomization and the
     * random seed parameter passed to them.
     */
    void setColorRandomization(bool randomize);

    /** Returns true if color randomization is enabled. */
    bool isColorRandomizationEnabled() const;

    static const QColor defaultTable[]; // table of default color entries

    static QString colorNameForIndex(int index);
    static QString translatedColorNameForIndex(int index);

private:
    // returns the active color table.  if none has been set specifically,
    // this is the default color table.
    const QColor *colorTable() const;

    // reads a single color entry from a KConfig source
    // and sets the palette entry at 'index' to the entry read.
    void readColorEntry(const KConfig &config, int index);
    // writes a single color entry to a KConfig source
    void writeColorEntry(KConfig &config, int index) const;

    // sets the amount of randomization allowed for a particular color
    // in the palette.  creates the randomization table if
    // it does not already exist
    void setRandomizationRange(int index, double hue, double saturation, double lightness);

    QString _description;
    QString _name;

    // pointer to custom color table, or 0 if the default color table is
    // being used
    QColor *_table;

    // pointer to randomization table, or 0 if no colors in the color
    // scheme support randomization
    RandomizationRange *_randomTable;

    qreal _opacity;

    // enables blur behind the terminal window
    bool _blur;

    bool _colorRandomization;

    ColorSchemeWallpaper::Ptr _wallpaper;

    static const char *const colorNames[TABLE_COLORS];
#if KI18N_VERSION >= QT_VERSION_CHECK(5, 89, 0)
    static const KLazyLocalizedString translatedColorNames[TABLE_COLORS];
#else
    static const char *const translatedColorNames[TABLE_COLORS];
#endif
};
}

Q_DECLARE_METATYPE(std::shared_ptr<const Konsole::ColorScheme>)

#endif // COLORSCHEME_H
