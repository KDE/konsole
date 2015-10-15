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
#include "KeyboardTranslatorManager.h"

// Qt
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QDir>

// KDE
#include <QDebug>
#include <QStandardPaths>

using namespace Konsole;

KeyboardTranslatorManager::KeyboardTranslatorManager()
    : _haveLoadedAll(false)
    , _fallbackTranslator(0)
{
    _fallbackTranslator = new FallbackKeyboardTranslator();
}

KeyboardTranslatorManager::~KeyboardTranslatorManager()
{
    qDeleteAll(_translators);
    delete _fallbackTranslator;
}

Q_GLOBAL_STATIC(KeyboardTranslatorManager, theKeyboardTranslatorManager)
KeyboardTranslatorManager* KeyboardTranslatorManager::instance()
{
    return theKeyboardTranslatorManager;
}

void KeyboardTranslatorManager::addTranslator(KeyboardTranslator* translator)
{
    _translators.insert(translator->name(), translator);

    if (!saveTranslator(translator))
        qWarning() << "Unable to save translator" << translator->name()
                   << "to disk.";
}

bool KeyboardTranslatorManager::deleteTranslator(const QString& name)
{
    Q_ASSERT(_translators.contains(name));

    // locate and delete
    QString path = findTranslatorPath(name);
    if (QFile::remove(path)) {
        _translators.remove(name);
        return true;
    } else {
        qWarning() << "Failed to remove translator - " << path;
        return false;
    }
}

QString KeyboardTranslatorManager::findTranslatorPath(const QString& name)
{
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("konsole/") + name + QStringLiteral(".keytab"));
}

void KeyboardTranslatorManager::findTranslators()
{

    QStringList list;
    const QStringList dirs = QStandardPaths::locateAll(QStandardPaths::GenericDataLocation, QStringLiteral("konsole"), QStandardPaths::LocateDirectory);
    list.reserve(dirs.size());

    Q_FOREACH (const QString& dir, dirs) {
        const QStringList fileNames = QDir(dir).entryList(QStringList() << QStringLiteral("*.keytab"));
        Q_FOREACH (const QString& file, fileNames) {
            list.append(dir + '/' + file);
        }
    }

    // add the name of each translator to the list and associated
    // the name with a null pointer to indicate that the translator
    // has not yet been loaded from disk
    foreach(const QString& translatorPath, list) {
        QString name = QFileInfo(translatorPath).baseName();

        if (!_translators.contains(name))
            _translators.insert(name, 0);
    }

    _haveLoadedAll = true;
}

const KeyboardTranslator* KeyboardTranslatorManager::findTranslator(const QString& name)
{
    if (name.isEmpty())
        return defaultTranslator();

    if (_translators.contains(name) && _translators[name] != 0)
        return _translators[name];

    KeyboardTranslator* translator = loadTranslator(name);

    if (translator != 0)
        _translators[name] = translator;
    else if (!name.isEmpty())
        qWarning() << "Unable to load translator" << name;

    return translator;
}

bool KeyboardTranslatorManager::saveTranslator(const KeyboardTranslator* translator)
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/konsole/");
    QDir().mkpath(dir);
    const QString path = dir + translator->name() + QStringLiteral(".keytab");

    ////qDebug() << "Saving translator to" << path;

    QFile destination(path);
    if (!destination.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Unable to save keyboard translation:"
                   << destination.errorString();
        return false;
    }

    {
        KeyboardTranslatorWriter writer(&destination);
        writer.writeHeader(translator->description());

        foreach(const KeyboardTranslator::Entry& entry, translator->entries()) {
            writer.writeEntry(entry);
        }
    }

    destination.close();

    return true;
}

KeyboardTranslator* KeyboardTranslatorManager::loadTranslator(const QString& name)
{
    const QString& path = findTranslatorPath(name);

    QFile source(path);
    if (name.isEmpty() || !source.open(QIODevice::ReadOnly | QIODevice::Text))
        return 0;

    return loadTranslator(&source, name);
}

KeyboardTranslator* KeyboardTranslatorManager::loadTranslator(QIODevice* source, const QString& name)
{
    KeyboardTranslator* translator = new KeyboardTranslator(name);
    KeyboardTranslatorReader reader(source);
    translator->setDescription(reader.description());
    while (reader.hasNextEntry())
        translator->addEntry(reader.nextEntry());

    source->close();

    if (!reader.parseError()) {
        return translator;
    } else {
        delete translator;
        return 0;
    }
}

const KeyboardTranslator* KeyboardTranslatorManager::defaultTranslator()
{
    // Try to find the default.keytab file if it exists, otherwise
    // fall back to the internal hard-coded fallback translator
    const KeyboardTranslator* translator = findTranslator(QStringLiteral("default"));
    if (!translator) {
        translator = _fallbackTranslator;
    }
    return translator;
}

QStringList KeyboardTranslatorManager::allTranslators()
{
    if (!_haveLoadedAll) {
        findTranslators();
    }

    return _translators.keys();
}
