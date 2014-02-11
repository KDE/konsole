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
#include "ColorScheme.h"

// Qt
#include <QtGui/QPainter>

// KDE
#include <KConfig>
#include <KLocalizedString>
#include <KConfigGroup>

namespace
{
const int FGCOLOR_INDEX = 0;
const int BGCOLOR_INDEX = 1;
}

using namespace Konsole;

// The following are almost IBM standard color codes, with some slight
// gamma correction for the dim colors to compensate for bright X screens.
// It contains the 8 ansiterm/xterm colors in 2 intensities.
const ColorEntry ColorScheme::defaultTable[TABLE_COLORS] = {
    ColorEntry(QColor(0x00, 0x00, 0x00)), // Dfore
    ColorEntry(QColor(0xFF, 0xFF, 0xFF)), // Dback
    ColorEntry(QColor(0x00, 0x00, 0x00)), // Black
    ColorEntry(QColor(0xB2, 0x18, 0x18)), // Red
    ColorEntry(QColor(0x18, 0xB2, 0x18)), // Green
    ColorEntry(QColor(0xB2, 0x68, 0x18)), // Yellow
    ColorEntry(QColor(0x18, 0x18, 0xB2)), // Blue
    ColorEntry(QColor(0xB2, 0x18, 0xB2)), // Magenta
    ColorEntry(QColor(0x18, 0xB2, 0xB2)), // Cyan
    ColorEntry(QColor(0xB2, 0xB2, 0xB2)), // White
    // intensive versions
    ColorEntry(QColor(0x00, 0x00, 0x00)),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF)),
    ColorEntry(QColor(0x68, 0x68, 0x68)),
    ColorEntry(QColor(0xFF, 0x54, 0x54)),
    ColorEntry(QColor(0x54, 0xFF, 0x54)),
    ColorEntry(QColor(0xFF, 0xFF, 0x54)),
    ColorEntry(QColor(0x54, 0x54, 0xFF)),
    ColorEntry(QColor(0xFF, 0x54, 0xFF)),
    ColorEntry(QColor(0x54, 0xFF, 0xFF)),
    ColorEntry(QColor(0xFF, 0xFF, 0xFF))
};

const char* const ColorScheme::colorNames[TABLE_COLORS] = {
    "Foreground",
    "Background",
    "Color0",
    "Color1",
    "Color2",
    "Color3",
    "Color4",
    "Color5",
    "Color6",
    "Color7",
    "ForegroundIntense",
    "BackgroundIntense",
    "Color0Intense",
    "Color1Intense",
    "Color2Intense",
    "Color3Intense",
    "Color4Intense",
    "Color5Intense",
    "Color6Intense",
    "Color7Intense"
};
const char* const ColorScheme::translatedColorNames[TABLE_COLORS] = {
    I18N_NOOP2("@item:intable palette", "Foreground"),
    I18N_NOOP2("@item:intable palette", "Background"),
    I18N_NOOP2("@item:intable palette", "Color 1"),
    I18N_NOOP2("@item:intable palette", "Color 2"),
    I18N_NOOP2("@item:intable palette", "Color 3"),
    I18N_NOOP2("@item:intable palette", "Color 4"),
    I18N_NOOP2("@item:intable palette", "Color 5"),
    I18N_NOOP2("@item:intable palette", "Color 6"),
    I18N_NOOP2("@item:intable palette", "Color 7"),
    I18N_NOOP2("@item:intable palette", "Color 8"),
    I18N_NOOP2("@item:intable palette", "Foreground (Intense)"),
    I18N_NOOP2("@item:intable palette", "Background (Intense)"),
    I18N_NOOP2("@item:intable palette", "Color 1 (Intense)"),
    I18N_NOOP2("@item:intable palette", "Color 2 (Intense)"),
    I18N_NOOP2("@item:intable palette", "Color 3 (Intense)"),
    I18N_NOOP2("@item:intable palette", "Color 4 (Intense)"),
    I18N_NOOP2("@item:intable palette", "Color 5 (Intense)"),
    I18N_NOOP2("@item:intable palette", "Color 6 (Intense)"),
    I18N_NOOP2("@item:intable palette", "Color 7 (Intense)"),
    I18N_NOOP2("@item:intable palette", "Color 8 (Intense)")
};

QString ColorScheme::colorNameForIndex(int index)
{
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    return QString(colorNames[index]);
}

QString ColorScheme::translatedColorNameForIndex(int index)
{
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    return i18nc("@item:intable palette", translatedColorNames[index]);
}

ColorScheme::ColorScheme()
{
    _table = 0;
    _randomTable = 0;
    _opacity = 1.0;
    setWallpaper(QString());
}

ColorScheme::ColorScheme(const ColorScheme& other)
    : _table(0)
    , _randomTable(0)
    , _opacity(other._opacity)
    , _wallpaper(other._wallpaper)
{
    setName(other.name());
    setDescription(other.description());

    if (other._table != 0) {
        for (int i = 0 ; i < TABLE_COLORS ; i++)
            setColorTableEntry(i, other._table[i]);
    }

    if (other._randomTable != 0) {
        for (int i = 0 ; i < TABLE_COLORS ; i++) {
            const RandomizationRange& range = other._randomTable[i];
            setRandomizationRange(i, range.hue, range.saturation, range.value);
        }
    }
}
ColorScheme::~ColorScheme()
{
    delete[] _table;
    delete[] _randomTable;
}

void ColorScheme::setDescription(const QString& aDescription)
{
    _description = aDescription;
}
QString ColorScheme::description() const
{
    return _description;
}

void ColorScheme::setName(const QString& aName)
{
    _name = aName;
}
QString ColorScheme::name() const
{
    return _name;
}

void ColorScheme::setColorTableEntry(int index , const ColorEntry& entry)
{
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    if (!_table) {
        _table = new ColorEntry[TABLE_COLORS];

        for (int i = 0; i < TABLE_COLORS; i++)
            _table[i] = defaultTable[i];
    }

    _table[index] = entry;
}
ColorEntry ColorScheme::colorEntry(int index , uint randomSeed) const
{
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    if (randomSeed != 0)
        qsrand(randomSeed);

    ColorEntry entry = colorTable()[index];

    if (randomSeed != 0 &&
            _randomTable != 0 &&
            !_randomTable[index].isNull()) {
        const RandomizationRange& range = _randomTable[index];

        int hueDifference = range.hue ? (qrand() % range.hue) - range.hue / 2 : 0;
        int saturationDifference = range.saturation ? (qrand() % range.saturation) - range.saturation / 2 : 0;
        int  valueDifference = range.value ? (qrand() % range.value) - range.value / 2 : 0;

        QColor& color = entry.color;

        int newHue = qAbs((color.hue() + hueDifference) % MAX_HUE);
        int newValue = qMin(qAbs(color.value() + valueDifference) , 255);
        int newSaturation = qMin(qAbs(color.saturation() + saturationDifference) , 255);

        color.setHsv(newHue, newSaturation, newValue);
    }

    return entry;
}
void ColorScheme::getColorTable(ColorEntry* table , uint randomSeed) const
{
    for (int i = 0 ; i < TABLE_COLORS ; i++)
        table[i] = colorEntry(i, randomSeed);
}
bool ColorScheme::randomizedBackgroundColor() const
{
    return _randomTable == 0 ? false : !_randomTable[BGCOLOR_INDEX].isNull();
}
void ColorScheme::setRandomizedBackgroundColor(bool randomize)
{
    // the hue of the background color is allowed to be randomly
    // adjusted as much as possible.
    //
    // the value and saturation are left alone to maintain read-ability
    if (randomize) {
        setRandomizationRange(BGCOLOR_INDEX , MAX_HUE , 255 , 0);
    } else {
        if (_randomTable)
            setRandomizationRange(BGCOLOR_INDEX , 0 , 0 , 0);
    }
}

void ColorScheme::setRandomizationRange(int index , quint16 hue , quint8 saturation ,
                                        quint8 value)
{
    Q_ASSERT(hue <= MAX_HUE);
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    if (_randomTable == 0)
        _randomTable = new RandomizationRange[TABLE_COLORS];

    _randomTable[index].hue = hue;
    _randomTable[index].value = value;
    _randomTable[index].saturation = saturation;
}

const ColorEntry* ColorScheme::colorTable() const
{
    if (_table)
        return _table;
    else
        return defaultTable;
}
QColor ColorScheme::foregroundColor() const
{
    return colorTable()[FGCOLOR_INDEX].color;
}
QColor ColorScheme::backgroundColor() const
{
    return colorTable()[BGCOLOR_INDEX].color;
}
bool ColorScheme::hasDarkBackground() const
{
    // value can range from 0 - 255, with larger values indicating higher brightness.
    // so 127 is in the middle, anything less is deemed 'dark'
    return backgroundColor().value() < 127;
}
void ColorScheme::setOpacity(qreal aOpacity)
{
    _opacity = aOpacity;
}
qreal ColorScheme::opacity() const
{
    return _opacity;
}

void ColorScheme::read(const KConfig& config)
{
    KConfigGroup configGroup = config.group("General");

    const QString schemeDescription = configGroup.readEntry("Description", I18N_NOOP("Un-named Color Scheme"));

    _description = i18n(schemeDescription.toUtf8());
    _opacity = configGroup.readEntry("Opacity", qreal(1.0));
    setWallpaper(configGroup.readEntry("Wallpaper", QString()));

    for (int i = 0 ; i < TABLE_COLORS ; i++) {
        readColorEntry(config, i);
    }
}

void ColorScheme::readColorEntry(const KConfig& config , int index)
{
    KConfigGroup configGroup = config.group(colorNameForIndex(index));

    ColorEntry entry;

    entry.color = configGroup.readEntry("Color", QColor());

    setColorTableEntry(index , entry);

    const quint16 hue = configGroup.readEntry("MaxRandomHue", 0);
    const quint8 value = configGroup.readEntry("MaxRandomValue", 0);
    const quint8 saturation = configGroup.readEntry("MaxRandomSaturation", 0);

    if (hue != 0 || value != 0 || saturation != 0)
        setRandomizationRange(index , hue , saturation , value);
}

void ColorScheme::write(KConfig& config) const
{
    KConfigGroup configGroup = config.group("General");

    configGroup.writeEntry("Description", _description);
    configGroup.writeEntry("Opacity", _opacity);
    configGroup.writeEntry("Wallpaper", _wallpaper->path());

    for (int i = 0 ; i < TABLE_COLORS ; i++) {
        writeColorEntry(config, i);
    }
}

void ColorScheme::writeColorEntry(KConfig& config , int index) const
{
    KConfigGroup configGroup = config.group(colorNameForIndex(index));

    const ColorEntry& entry = colorTable()[index];

    configGroup.writeEntry("Color", entry.color);

    // Remove unused keys
    if (configGroup.hasKey("Transparent")) {
        configGroup.deleteEntry("Transparent");
    }
    if (configGroup.hasKey("Transparency")) {
        configGroup.deleteEntry("Transparency");
    }
    if (configGroup.hasKey("Bold")) {
        configGroup.deleteEntry("Bold");
    }

    RandomizationRange random = _randomTable != 0 ? _randomTable[index] : RandomizationRange();

    // record randomization if this color has randomization or
    // if one of the keys already exists
    if (!random.isNull() || configGroup.hasKey("MaxRandomHue")) {
        configGroup.writeEntry("MaxRandomHue", static_cast<int>(random.hue));
        configGroup.writeEntry("MaxRandomValue", static_cast<int>(random.value));
        configGroup.writeEntry("MaxRandomSaturation", static_cast<int>(random.saturation));
    }
}

void ColorScheme::setWallpaper(const QString& path)
{
    _wallpaper = new ColorSchemeWallpaper(path);
}

ColorSchemeWallpaper::Ptr ColorScheme::wallpaper() const
{
    return _wallpaper;
}

ColorSchemeWallpaper::ColorSchemeWallpaper(const QString& aPath)
    : _path(aPath),
      _picture(0)
{
}

ColorSchemeWallpaper::~ColorSchemeWallpaper()
{
    delete _picture;
}

void ColorSchemeWallpaper::load()
{
    if (_path.isEmpty())
        return;

    // Create and load original pixmap
    if (!_picture)
        _picture = new QPixmap();

    if (_picture->isNull())
        _picture->load(_path);
}

bool ColorSchemeWallpaper::isNull() const
{
    return _path.isEmpty();
}

bool ColorSchemeWallpaper::draw(QPainter& painter, const QRect& rect, qreal opacity)
{
    if (!_picture || _picture->isNull())
        return false;

    if (qFuzzyCompare(1.0, opacity)) {
        painter.drawTiledPixmap(rect, *_picture, rect.topLeft());
        return true;
    }
    painter.save();
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(rect, QColor(0,0,0,0));
    painter.setOpacity(opacity);
    painter.drawTiledPixmap(rect, *_picture, rect.topLeft());
    painter.restore();
    return true;
}

QString ColorSchemeWallpaper::path() const
{
    return _path;
}

