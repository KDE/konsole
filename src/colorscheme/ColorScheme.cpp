/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ColorScheme.h"
#include "RandomizationRange.h"
#include "hsluv.h"

// Qt
#include <QPainter>

// KDE
#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>

// STL
#include <random>

// Konsole
#include "colorschemedebug.h"

namespace
{
const int FGCOLOR_INDEX = 0;
const int BGCOLOR_INDEX = 1;

const char RandomHueRangeKey[] = "RandomHueRange";
const char RandomSaturationRangeKey[] = "RandomSaturationRange";
const char RandomLightnessRangeKey[] = "RandomLightnessRange";
const char EnableColorRandomizationKey[] = "ColorRandomization";

const double MaxHue = 360.0;
const double MaxSaturation = 100.0;
const double MaxLightness = 100.0;
}

using namespace Konsole;

// The following are almost IBM standard color codes, with some slight
// gamma correction for the dim colors to compensate for bright X screens.
// It contains the 8 ansiterm/xterm colors in 2 intensities.
const QColor ColorScheme::defaultTable[TABLE_COLORS] = {
    QColor(0x00, 0x00, 0x00), // Dfore
    QColor(0xFF, 0xFF, 0xFF), // Dback
    QColor(0x00, 0x00, 0x00), // Black
    QColor(0xB2, 0x18, 0x18), // Red
    QColor(0x18, 0xB2, 0x18), // Green
    QColor(0xB2, 0x68, 0x18), // Yellow
    QColor(0x18, 0x18, 0xB2), // Blue
    QColor(0xB2, 0x18, 0xB2), // Magenta
    QColor(0x18, 0xB2, 0xB2), // Cyan
    QColor(0xB2, 0xB2, 0xB2), // White
    // intensive versions
    QColor(0x00, 0x00, 0x00),
    QColor(0xFF, 0xFF, 0xFF),
    QColor(0x68, 0x68, 0x68),
    QColor(0xFF, 0x54, 0x54),
    QColor(0x54, 0xFF, 0x54),
    QColor(0xFF, 0xFF, 0x54),
    QColor(0x54, 0x54, 0xFF),
    QColor(0xFF, 0x54, 0xFF),
    QColor(0x54, 0xFF, 0xFF),
    QColor(0xFF, 0xFF, 0xFF),
    // Here are faint intensities, which may not be good.
    // faint versions
    QColor(0x00, 0x00, 0x00),
    QColor(0xFF, 0xFF, 0xFF),
    QColor(0x00, 0x00, 0x00),
    QColor(0x65, 0x00, 0x00),
    QColor(0x00, 0x65, 0x00),
    QColor(0x65, 0x5E, 0x00),
    QColor(0x00, 0x00, 0x65),
    QColor(0x65, 0x00, 0x65),
    QColor(0x00, 0x65, 0x65),
    QColor(0x65, 0x65, 0x65),
};

const char *const ColorScheme::colorNames[TABLE_COLORS] = {
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
    "Color7Intense",
    "ForegroundFaint",
    "BackgroundFaint",
    "Color0Faint",
    "Color1Faint",
    "Color2Faint",
    "Color3Faint",
    "Color4Faint",
    "Color5Faint",
    "Color6Faint",
    "Color7Faint",
};
const char *const ColorScheme::translatedColorNames[TABLE_COLORS] = {
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
    I18N_NOOP2("@item:intable palette", "Color 8 (Intense)"),
    I18N_NOOP2("@item:intable palette", "Foreground (Faint)"),
    I18N_NOOP2("@item:intable palette", "Background (Faint)"),
    I18N_NOOP2("@item:intable palette", "Color 1 (Faint)"),
    I18N_NOOP2("@item:intable palette", "Color 2 (Faint)"),
    I18N_NOOP2("@item:intable palette", "Color 3 (Faint)"),
    I18N_NOOP2("@item:intable palette", "Color 4 (Faint)"),
    I18N_NOOP2("@item:intable palette", "Color 5 (Faint)"),
    I18N_NOOP2("@item:intable palette", "Color 6 (Faint)"),
    I18N_NOOP2("@item:intable palette", "Color 7 (Faint)"),
    I18N_NOOP2("@item:intable palette", "Color 8 (Faint)"),
};

QString ColorScheme::colorNameForIndex(int index)
{
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    return QString(QLatin1String(colorNames[index]));
}

QString ColorScheme::translatedColorNameForIndex(int index)
{
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    return i18nc("@item:intable palette", translatedColorNames[index]);
}

ColorScheme::ColorScheme()
    : _description(QString())
    , _name(QString())
    , _table(nullptr)
    , _randomTable(nullptr)
    , _opacity(1.0)
    , _blur(false)
    , _colorRandomization(false)
    , _wallpaper(nullptr)
{
    setWallpaper(QString());
}

ColorScheme::ColorScheme(const ColorScheme &other)
    : _description(QString())
    , _name(QString())
    , _table(nullptr)
    , _randomTable(nullptr)
    , _opacity(other._opacity)
    , _blur(other._blur)
    , _colorRandomization(other._colorRandomization)
    , _wallpaper(other._wallpaper)
{
    setName(other.name());
    setDescription(other.description());

    if (other._table != nullptr) {
        for (int i = 0; i < TABLE_COLORS; i++) {
            setColorTableEntry(i, other._table[i]);
        }
    }

    if (other._randomTable != nullptr) {
        for (int i = 0; i < TABLE_COLORS; i++) {
            const RandomizationRange &range = other._randomTable[i];
            setRandomizationRange(i, range.hue, range.saturation, range.lightness);
        }
    }
}

ColorScheme::~ColorScheme()
{
    delete[] _table;
    delete[] _randomTable;
}

void ColorScheme::setDescription(const QString &description)
{
    _description = description;
}

QString ColorScheme::description() const
{
    return _description;
}

void ColorScheme::setName(const QString &name)
{
    _name = name;
}

QString ColorScheme::name() const
{
    return _name;
}

void ColorScheme::setColorTableEntry(int index, const QColor &entry)
{
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    if (_table == nullptr) {
        _table = new QColor[TABLE_COLORS];

        std::copy_n(defaultTable, TABLE_COLORS, _table);
    }

    if (entry.isValid()) {
        _table[index] = entry;
    } else {
        _table[index] = defaultTable[index];
        qCDebug(ColorSchemeDebug) << "ColorScheme" << name() << "has an invalid color index" << index << ", using default table color";
    }
}

QColor ColorScheme::colorEntry(int index, uint randomSeed) const
{
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    QColor entry = colorTable()[index];

    if (!_colorRandomization || randomSeed == 0 || _randomTable == nullptr || _randomTable[index].isNull()) {
        return entry;
    }

    double baseHue;
    double baseSaturation;
    double baseLightness;
    rgb2hsluv(entry.redF(), entry.greenF(), entry.blueF(), &baseHue, &baseSaturation, &baseLightness);

    const RandomizationRange &range = _randomTable[index];

    // 32-bit Mersenne Twister
    // Can't use default_random_engine, because in GCC this maps to
    // minstd_rand0 which always gives us 0 on the first number.
    std::mt19937 randomEngine(randomSeed);

    // Use hues located around base color's hue.
    // H=0 [|=      =]    H=128 [   =|=   ]    H=360 [=      =|]
    const double minHue = baseHue - range.hue / 2.0;
    const double maxHue = baseHue + range.hue / 2.0;
    std::uniform_real_distribution<> hueDistribution(minHue, maxHue);
    // Hue value is an angle, it wraps after 360°. Adding MAX_HUE
    // guarantees that the sum is not negative.
    const double hue = fmod(MaxHue + hueDistribution(randomEngine), MaxHue);

    // Saturation is always decreased. With more saturation more
    // information about hue is preserved in RGB color space
    // (consider red with S=100 and "red" with S=0 which is gray).
    // Additionally, I think it can be easier to imagine more
    // toned color than more vivid one.
    // S=0 [|==      ]    S=50 [  ==|    ]    S=100 [      ==|]
    const double minSaturation = qMax(baseSaturation - range.saturation, 0.0);
    const double maxSaturation = qMax(range.saturation, baseSaturation);
    // Use rising linear distribution as colors with lower
    // saturation are less distinguishable.
    double saturation;
    if (qFuzzyCompare(minSaturation, maxSaturation)) {
        saturation = baseSaturation;
    } else {
        std::piecewise_linear_distribution<> saturationDistribution({minSaturation, maxSaturation}, [](double v) {
            return v;
        });
        saturation = saturationDistribution(randomEngine);
    }

    // Lightness range has base value at its center. The base
    // value is clamped to prevent the range from shrinking.
    // L=0 [=|=        ]    L=50 [    =|=    ]    L=100 [        =|=]
    baseLightness = qBound(range.lightness / 2.0, baseLightness, MaxLightness - range.lightness);
    const double minLightness = qMax(baseLightness - range.lightness / 2.0, 0.0);
    const double maxLightness = qMin(baseLightness + range.lightness / 2.0, MaxLightness);
    // Use triangular distribution with peak at L=50.0.
    // Dark and very light colors are less distinguishable.
    double lightness;
    if (qFuzzyCompare(minLightness, maxLightness)) {
        lightness = baseLightness;
    } else {
        static const auto lightnessWeightsFunc = [](double v) {
            return 50.0 - qAbs(v - 50.0);
        };
        std::piecewise_linear_distribution<> lightnessDistribution;
        if (minLightness < 50.0 && 50.0 < maxLightness) {
            lightnessDistribution = std::piecewise_linear_distribution<>({minLightness, 50.0, maxLightness}, lightnessWeightsFunc);
        } else {
            lightnessDistribution = std::piecewise_linear_distribution<>({minLightness, maxLightness}, lightnessWeightsFunc);
        }
        lightness = lightnessDistribution(randomEngine);
    }

    double red;
    double green;
    double blue;
    hsluv2rgb(hue, saturation, lightness, &red, &green, &blue);

    return {qRound(red * 255), qRound(green * 255), qRound(blue * 255)};
}

void ColorScheme::getColorTable(QColor *table, uint randomSeed) const
{
    for (int i = 0; i < TABLE_COLORS; i++) {
        table[i] = colorEntry(i, randomSeed);
    }
}

bool ColorScheme::isColorRandomizationEnabled() const
{
    return (_colorRandomization && _randomTable != nullptr);
}

void ColorScheme::setColorRandomization(bool randomize)
{
    _colorRandomization = randomize;
    if (randomize) {
        bool hasAnyRandomizationEntries = false;
        if (_randomTable != nullptr) {
            for (int i = 0; !hasAnyRandomizationEntries && i < TABLE_COLORS; i++) {
                hasAnyRandomizationEntries = !_randomTable[i].isNull();
            }
        }
        // Set default randomization settings
        if (!hasAnyRandomizationEntries) {
            static const int ColorIndexesForRandomization[] = {
                ColorFgIndex,
                ColorBgIndex,
                ColorFgIntenseIndex,
                ColorBgIntenseIndex,
                ColorFgFaintIndex,
                ColorBgFaintIndex,
            };
            for (int index : ColorIndexesForRandomization) {
                setRandomizationRange(index, MaxHue, MaxSaturation, 0.0);
            }
        }
    }
}

void ColorScheme::setRandomizationRange(int index, double hue, double saturation, double lightness)
{
    Q_ASSERT(hue <= MaxHue);
    Q_ASSERT(index >= 0 && index < TABLE_COLORS);

    if (_randomTable == nullptr) {
        _randomTable = new RandomizationRange[TABLE_COLORS];
    }

    _randomTable[index].hue = hue;
    _randomTable[index].saturation = saturation;
    _randomTable[index].lightness = lightness;
}

const QColor *ColorScheme::colorTable() const
{
    if (_table != nullptr) {
        return _table;
    }
    return defaultTable;
}

QColor ColorScheme::foregroundColor() const
{
    return colorTable()[FGCOLOR_INDEX];
}

QColor ColorScheme::backgroundColor() const
{
    return colorTable()[BGCOLOR_INDEX];
}

bool ColorScheme::hasDarkBackground() const
{
    double h;
    double s;
    double l;
    const double r = backgroundColor().redF();
    const double g = backgroundColor().greenF();
    const double b = backgroundColor().blueF();
    rgb2hsluv(r, g, b, &h, &s, &l);
    return l < 50;
}

void ColorScheme::setOpacity(qreal opacity)
{
    if (opacity < 0.0 || opacity > 1.0) {
        qCDebug(ColorSchemeDebug) << "ColorScheme" << name() << "has an invalid opacity" << opacity << "using 1";
        opacity = 1.0;
    }
    _opacity = opacity;
}

qreal ColorScheme::opacity() const
{
    return _opacity;
}

void ColorScheme::setBlur(bool blur)
{
    _blur = blur;
}

bool ColorScheme::blur() const
{
    return _blur;
}

void ColorScheme::read(const KConfig &config)
{
    KConfigGroup configGroup = config.group("General");

    const QString schemeDescription = configGroup.readEntry("Description", i18nc("@item", "Un-named Color Scheme"));

    _description = i18n(schemeDescription.toUtf8().constData());
    setOpacity(configGroup.readEntry("Opacity", 1.0));
    _blur = configGroup.readEntry("Blur", false);
    setWallpaper(configGroup.readEntry("Wallpaper", QString()));
    _colorRandomization = configGroup.readEntry(EnableColorRandomizationKey, false);

    for (int i = 0; i < TABLE_COLORS; i++) {
        readColorEntry(config, i);
    }
}

void ColorScheme::readColorEntry(const KConfig &config, int index)
{
    KConfigGroup configGroup = config.group(colorNameForIndex(index));

    if (!configGroup.hasKey("Color") && _table != nullptr) {
        setColorTableEntry(index, _table[index % BASE_COLORS]);
        return;
    }

    QColor entry;

    entry = configGroup.readEntry("Color", QColor());
    setColorTableEntry(index, entry);

    const auto readAndCheckConfigEntry = [&](const char *key, double min, double max) -> double {
        const double value = configGroup.readEntry(key, min);
        if (min > value || value > max) {
            qCDebug(ColorSchemeDebug) << QStringLiteral(
                                             "Color scheme \"%1\": color index 2 has an invalid value: %3 = %4. "
                                             "Allowed value range: %5 - %6. Using %7.")
                                             .arg(name())
                                             .arg(index)
                                             .arg(QLatin1String(key))
                                             .arg(value, 0, 'g', 1)
                                             .arg(min, 0, 'g', 1)
                                             .arg(max, 0, 'g', 1)
                                             .arg(min, 0, 'g', 1);
            return min;
        }
        return value;
    };

    double hue = readAndCheckConfigEntry(RandomHueRangeKey, 0.0, MaxHue);
    double saturation = readAndCheckConfigEntry(RandomSaturationRangeKey, 0.0, MaxSaturation);
    double lightness = readAndCheckConfigEntry(RandomLightnessRangeKey, 0.0, MaxLightness);

    if (!qFuzzyIsNull(hue) || !qFuzzyIsNull(saturation) || !qFuzzyIsNull(lightness)) {
        setRandomizationRange(index, hue, saturation, lightness);
    }
}

void ColorScheme::write(KConfig &config) const
{
    KConfigGroup configGroup = config.group("General");

    configGroup.writeEntry("Description", _description);
    configGroup.writeEntry("Opacity", _opacity);
    configGroup.writeEntry("Blur", _blur);
    configGroup.writeEntry("Wallpaper", _wallpaper->path());
    configGroup.writeEntry(EnableColorRandomizationKey, _colorRandomization);

    for (int i = 0; i < TABLE_COLORS; i++) {
        writeColorEntry(config, i);
    }
}

void ColorScheme::writeColorEntry(KConfig &config, int index) const
{
    KConfigGroup configGroup = config.group(colorNameForIndex(index));

    const QColor &entry = colorTable()[index];

    configGroup.writeEntry("Color", entry);

    // Remove unused keys
    static const char *obsoleteKeys[] = {
        "Transparent",
        "Transparency",
        "Bold",
        // Uncomment when people stop using Konsole from 2019:
        // "MaxRandomHue",
        // "MaxRandomValue",
        // "MaxRandomSaturation"
    };
    for (const auto key : obsoleteKeys) {
        if (configGroup.hasKey(key)) {
            configGroup.deleteEntry(key);
        }
    }

    RandomizationRange random = _randomTable != nullptr ? _randomTable[index] : RandomizationRange();

    const auto checkAndMaybeSaveValue = [&](const char *key, double value) {
        const bool valueIsNull = qFuzzyCompare(value, 0.0);
        const bool keyExists = configGroup.hasKey(key);
        const bool keyExistsAndHasDifferentValue = !qFuzzyCompare(configGroup.readEntry(key, value), value);
        if ((!valueIsNull && !keyExists) || keyExistsAndHasDifferentValue) {
            configGroup.writeEntry(key, value);
        }
    };

    checkAndMaybeSaveValue(RandomHueRangeKey, random.hue);
    checkAndMaybeSaveValue(RandomSaturationRangeKey, random.saturation);
    checkAndMaybeSaveValue(RandomLightnessRangeKey, random.lightness);
}

void ColorScheme::setWallpaper(const QString &path)
{
    _wallpaper = new ColorSchemeWallpaper(path);
}

ColorSchemeWallpaper::Ptr ColorScheme::wallpaper() const
{
    return _wallpaper;
}
