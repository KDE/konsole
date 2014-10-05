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
#include "ColorSchemeManager.h"

// Qt
#include <QtCore/QIODevice>
#include <QtCore/QFileInfo>
#include <QtCore/QFile>

// KDE
#include <KStandardDirs>
#include <KGlobal>
#include <KConfig>
#include <KLocalizedString>
#include <KDebug>

using namespace Konsole;

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
    explicit KDE3ColorSchemeReader(QIODevice* device);

    /**
     * Reads and parses the contents of the .schema file from the input
     * device and returns the ColorScheme defined within it.
     *
     * Returns a null pointer if an error occurs whilst parsing
     * the contents of the file.
     */
    ColorScheme* read();

private:
    // reads a line from the file specifying a color palette entry
    // format is: color [index] [red] [green] [blue] [transparent] [bold]
    bool readColorLine(const QString& line , ColorScheme* scheme);
    bool readTitleLine(const QString& line , ColorScheme* scheme);

    QIODevice* _device;
};

KDE3ColorSchemeReader::KDE3ColorSchemeReader(QIODevice* device) :
    _device(device)
{
}
ColorScheme* KDE3ColorSchemeReader::read()
{
    Q_ASSERT(_device->openMode() == QIODevice::ReadOnly ||
             _device->openMode() == QIODevice::ReadWrite);

    ColorScheme* scheme = new ColorScheme();

    QRegExp comment("#.*$");
    while (!_device->atEnd()) {
        QString line(_device->readLine());
        line.remove(comment);
        line = line.simplified();

        if (line.isEmpty())
            continue;

        if (line.startsWith(QLatin1String("color"))) {
            if (!readColorLine(line, scheme))
                kWarning() << "Failed to read KDE 3 color scheme line" << line;
        } else if (line.startsWith(QLatin1String("title"))) {
            if (!readTitleLine(line, scheme))
                kWarning() << "Failed to read KDE 3 color scheme title line" << line;
        } else {
            kWarning() << "KDE 3 color scheme contains an unsupported feature, '" <<
                       line << "'";
        }
    }

    return scheme;
}
bool KDE3ColorSchemeReader::readColorLine(const QString& line, ColorScheme* scheme)
{
    QStringList list = line.split(QChar(' '));

    if (list.count() != 7)
        return false;
    if (list.first() != "color")
        return false;

    int index = list[1].toInt();
    int red = list[2].toInt();
    int green = list[3].toInt();
    int blue = list[4].toInt();
    int transparent = list[5].toInt();
    int bold = list[6].toInt();

    const int MAX_COLOR_VALUE = 255;

    if ((index < 0 || index >= TABLE_COLORS)
            || (red < 0 || red > MAX_COLOR_VALUE)
            || (blue < 0 || blue > MAX_COLOR_VALUE)
            || (green < 0 || green > MAX_COLOR_VALUE)
            || (transparent != 0 && transparent != 1)
            || (bold != 0 && bold != 1))
        return false;

    ColorEntry entry;
    entry.color = QColor(red, green, blue);
    entry.fontWeight = (bold != 0) ? ColorEntry::Bold : ColorEntry::UseCurrentFormat;

    scheme->setColorTableEntry(index, entry);
    return true;
}
bool KDE3ColorSchemeReader::readTitleLine(const QString& line, ColorScheme* scheme)
{
    if (!line.startsWith(QLatin1String("title")))
        return false;

    int spacePos = line.indexOf(' ');
    if (spacePos == -1)
        return false;

    QString description = line.mid(spacePos + 1);

    scheme->setDescription(i18n(description.toUtf8().constData()));
    return true;
}

ColorSchemeManager::ColorSchemeManager()
    : _haveLoadedAll(false)
{
}

ColorSchemeManager::~ColorSchemeManager()
{
    qDeleteAll(_colorSchemes);
}

Q_GLOBAL_STATIC(ColorSchemeManager, theColorSchemeManager)

ColorSchemeManager* ColorSchemeManager::instance()
{
    return theColorSchemeManager;
}

void ColorSchemeManager::loadAllColorSchemes()
{
    int success = 0;
    int failed = 0;

    QStringList nativeColorSchemes = listColorSchemes();
    foreach(const QString& colorScheme, nativeColorSchemes) {
        if (loadColorScheme(colorScheme))
            success++;
        else
            failed++;
    }

    QStringList kde3ColorSchemes = listKDE3ColorSchemes();
    foreach(const QString& colorScheme, kde3ColorSchemes) {
        if (loadKDE3ColorScheme(colorScheme))
            success++;
        else
            failed++;
    }

    if (failed > 0)
        kWarning() << "failed to load " << failed << " color schemes.";

    _haveLoadedAll = true;
}

QList<const ColorScheme*> ColorSchemeManager::allColorSchemes()
{
    if (!_haveLoadedAll) {
        loadAllColorSchemes();
    }

    return _colorSchemes.values();
}

bool ColorSchemeManager::loadColorScheme(const QString& filePath)
{
    if (!filePath.endsWith(QLatin1String(".colorscheme")) || !QFile::exists(filePath))
        return false;

    QFileInfo info(filePath);

    KConfig config(filePath , KConfig::NoGlobals);
    ColorScheme* scheme = new ColorScheme();
    scheme->setName(info.baseName());
    scheme->read(config);

    if (scheme->name().isEmpty()) {
        kWarning() << "Color scheme in" << filePath << "does not have a valid name and was not loaded.";
        delete scheme;
        return false;
    }

    if (!_colorSchemes.contains(info.baseName())) {
        _colorSchemes.insert(scheme->name(), scheme);
    } else {
        kDebug() << "color scheme with name" << scheme->name() << "has already been" <<
                 "found, ignoring.";

        delete scheme;
    }

    return true;
}

bool ColorSchemeManager::loadKDE3ColorScheme(const QString& filePath)
{
    QFile file(filePath);
    if (!filePath.endsWith(QLatin1String(".schema")) || !file.open(QIODevice::ReadOnly))
        return false;

    KDE3ColorSchemeReader reader(&file);
    ColorScheme* scheme = reader.read();
    scheme->setName(QFileInfo(file).baseName());
    file.close();

    if (scheme->name().isEmpty()) {
        kWarning() << "color scheme name is not valid.";
        delete scheme;
        return false;
    }

    QFileInfo info(filePath);

    if (!_colorSchemes.contains(info.baseName())) {
        addColorScheme(scheme);
    } else {
        kWarning() << "color scheme with name" << scheme->name() << "has already been" <<
                   "found, ignoring.";
        delete scheme;
    }

    return true;
}

QStringList ColorSchemeManager::listColorSchemes()
{
    return KGlobal::dirs()->findAllResources("data",
            "konsole/*.colorscheme",
            KStandardDirs::NoDuplicates);
}

QStringList ColorSchemeManager::listKDE3ColorSchemes()
{
    return KGlobal::dirs()->findAllResources("data",
            "konsole/*.schema",
            KStandardDirs::NoDuplicates);
}

const ColorScheme ColorSchemeManager::_defaultColorScheme;
const ColorScheme* ColorSchemeManager::defaultColorScheme() const
{
    return &_defaultColorScheme;
}

void ColorSchemeManager::addColorScheme(ColorScheme* scheme)
{
    // remove existing colorscheme with the same name
    if (_colorSchemes.contains(scheme->name())) {
        delete _colorSchemes[scheme->name()];
        _colorSchemes.remove(scheme->name());
    }

    _colorSchemes.insert(scheme->name(), scheme);

    // save changes to disk
    QString path = KGlobal::dirs()->saveLocation("data", "konsole/") + scheme->name() + ".colorscheme";
    KConfig config(path , KConfig::NoGlobals);

    scheme->write(config);
}

bool ColorSchemeManager::deleteColorScheme(const QString& name)
{
    Q_ASSERT(_colorSchemes.contains(name));

    // look up the path and delete
    QString path = findColorSchemePath(name);
    if (QFile::remove(path)) {
        delete _colorSchemes[name];
        _colorSchemes.remove(name);
        return true;
    } else {
        kWarning() << "Failed to remove color scheme -" << path;
        return false;
    }
}

const ColorScheme* ColorSchemeManager::findColorScheme(const QString& name)
{
    if (name.isEmpty())
        return defaultColorScheme();

    // A fix to prevent infinite loops if users puts / in ColorScheme name
    // Konsole will create a sub-folder in that case (bko 315086)
    // More code will have to go in to prevent the users from doing that.
    if (name.contains("/")) {
        kWarning() << name << " has an invalid character / in the name ... skipping";
        return defaultColorScheme();
    }

    if (_colorSchemes.contains(name)) {
        return _colorSchemes[name];
    } else {
        // look for this color scheme
        QString path = findColorSchemePath(name);
        if (!path.isEmpty() && loadColorScheme(path)) {
            return findColorScheme(name);
        } else {
            if (!path.isEmpty() && loadKDE3ColorScheme(path))
                return findColorScheme(name);
        }

        kWarning() << "Could not find color scheme - " << name;

        return 0;
    }
}

QString ColorSchemeManager::findColorSchemePath(const QString& name) const
{
    QString path = KStandardDirs::locate("data", "konsole/" + name + ".colorscheme");

    if (!path.isEmpty())
        return path;

    path = KStandardDirs::locate("data", "konsole/" + name + ".schema");

    return path;
}

