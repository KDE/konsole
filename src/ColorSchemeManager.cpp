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

// Own
#include "ColorSchemeManager.h"

#include "konsoledebug.h"

// Qt
#include <QIODevice>
#include <QFileInfo>
#include <QFile>
#include <QDir>

// KDE
#include <KConfig>

using namespace Konsole;

ColorSchemeManager::ColorSchemeManager() :
    _colorSchemes(QHash<QString, const ColorScheme *>()),
    _haveLoadedAll(false)
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
    foreach (const QString &colorScheme, nativeColorSchemes) {
        if (loadColorScheme(colorScheme)) {
            success++;
        } else {
            failed++;
        }
    }

    if (failed > 0) {
        qCDebug(KonsoleDebug) << "failed to load " << failed << " color schemes.";
    }

    _haveLoadedAll = true;
}

QList<const ColorScheme *> ColorSchemeManager::allColorSchemes()
{
    if (!_haveLoadedAll) {
        loadAllColorSchemes();
    }

    return _colorSchemes.values();
}

bool ColorSchemeManager::loadColorScheme(const QString &filePath)
{
    if (!pathIsColorScheme(filePath) || !QFile::exists(filePath)) {
        return false;
    }

    auto name = colorSchemeNameFromPath(filePath);

    KConfig config(filePath, KConfig::NoGlobals);
    auto scheme = new ColorScheme();
    scheme->setName(name);
    scheme->read(config);

    if (scheme->name().isEmpty()) {
        qCDebug(KonsoleDebug) << "Color scheme in" << filePath
                              << "does not have a valid name and was not loaded.";
        delete scheme;
        return false;
    }

    if (!_colorSchemes.contains(name)) {
        _colorSchemes.insert(scheme->name(), scheme);
    } else {
        //qDebug() << "color scheme with name" << scheme->name() << "has already been" <<
        //         "found, ignoring.";

        delete scheme;
    }

    return true;
}

bool ColorSchemeManager::unloadColorScheme(const QString &filePath)
{
    if (!pathIsColorScheme(filePath)) {
        return false;
    }
    auto name = colorSchemeNameFromPath(filePath);
    delete _colorSchemes.take(name);
    return true;
}

QString ColorSchemeManager::colorSchemeNameFromPath(const QString &path)
{
    if (!pathIsColorScheme(path)) {
        return QString();
    }
    return QFileInfo(path).completeBaseName();
}

QStringList ColorSchemeManager::listColorSchemes()
{
    QStringList colorschemes;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("konsole"),
                                                       QStandardPaths::LocateDirectory);
    colorschemes.reserve(dirs.size());

    Q_FOREACH (const QString &dir, dirs) {
        const QStringList fileNames = QDir(dir).entryList(QStringList() << QStringLiteral("*.colorscheme"));
        Q_FOREACH (const QString &file, fileNames) {
            colorschemes.append(dir + QLatin1Char('/') + file);
        }
    }
    return colorschemes;
}

const ColorScheme ColorSchemeManager::_defaultColorScheme;
const ColorScheme *ColorSchemeManager::defaultColorScheme() const
{
    return &_defaultColorScheme;
}

void ColorSchemeManager::addColorScheme(ColorScheme *scheme)
{
    // remove existing colorscheme with the same name
    if (_colorSchemes.contains(scheme->name())) {
        delete _colorSchemes.take(scheme->name());
    }

    _colorSchemes.insert(scheme->name(), scheme);

    // save changes to disk

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
                        + QStringLiteral("/konsole/");
    QDir().mkpath(dir);
    const QString path = dir + scheme->name() + QStringLiteral(".colorscheme");
    KConfig config(path, KConfig::NoGlobals);

    scheme->write(config);
}

bool ColorSchemeManager::deleteColorScheme(const QString &name)
{
    Q_ASSERT(_colorSchemes.contains(name));

    // look up the path and delete
    QString path = findColorSchemePath(name);
    if (QFile::remove(path)) {
        delete _colorSchemes.take(name);
        return true;
    } else {
        qCDebug(KonsoleDebug)<<"Failed to remove color scheme -"<<path;
        return false;
    }
}

const ColorScheme *ColorSchemeManager::findColorScheme(const QString &name)
{
    if (name.isEmpty()) {
        return defaultColorScheme();
    }

    // A fix to prevent infinite loops if users puts / in ColorScheme name
    // Konsole will create a sub-folder in that case (bko 315086)
    // More code will have to go in to prevent the users from doing that.
    if (name.contains(QLatin1String("/"))) {
        qCDebug(KonsoleDebug)<<name<<" has an invalid character / in the name ... skipping";
        return defaultColorScheme();
    }

    if (_colorSchemes.contains(name)) {
        return _colorSchemes[name];
    } else {
        // look for this color scheme
        QString path = findColorSchemePath(name);
        if (!path.isEmpty() && loadColorScheme(path)) {
            return findColorScheme(name);
        }

        qCDebug(KonsoleDebug) << "Could not find color scheme - " << name;

        return nullptr;
    }
}

QString ColorSchemeManager::findColorSchemePath(const QString &name) const
{
    QString path = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("konsole/") + name + QStringLiteral(".colorscheme"));

    if (!path.isEmpty()) {
        return path;
    }

    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("konsole/") + name + QStringLiteral(".schema"));
}

bool ColorSchemeManager::pathIsColorScheme(const QString &path)
{
    return path.endsWith(QLatin1String(".colorscheme"));
}

bool ColorSchemeManager::isColorSchemeDeletable(const QString &name)
{
    const QString path = findColorSchemePath(name);

    QFileInfo fileInfo(path);
    QFileInfo dirInfo = fileInfo.path();

    return dirInfo.isWritable();
}

bool ColorSchemeManager::canResetColorScheme(const QString &name)
{
    const QStringList paths = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("konsole/") + name + QStringLiteral(".colorscheme"));

    // if the colorscheme exists in both a writable location under the
    // user's home dir and a system-wide location, then it's possible
    // to delete the colorscheme under the user's home dir so that the
    // colorscheme from the system-wide location can be used instead,
    // i.e. resetting the colorscheme
    return (paths.count() > 1);
}
