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

// Qt
#include <QFile>
#include <QFileInfo>

// KDE
#include <KConfig>
#include <KLocale>
#include <KStandardDirs>

// Konsole
#include "ColorScheme.h"
#include "TECommon.h"

using namespace Konsole;

const ColorEntry ColorScheme::defaultTable[TABLE_COLORS] =
 // The following are almost IBM standard color codes, with some slight
 // gamma correction for the dim colors to compensate for bright X screens.
 // It contains the 8 ansiterm/xterm colors in 2 intensities.
{
    ColorEntry( QColor(0x00,0x00,0x00), 0, 0 ), ColorEntry(
QColor(0xFF,0xFF,0xFF), 1, 0 ), // Dfore, Dback
    ColorEntry( QColor(0x00,0x00,0x00), 0, 0 ), ColorEntry(
QColor(0xB2,0x18,0x18), 0, 0 ), // Black, Red
    ColorEntry( QColor(0x18,0xB2,0x18), 0, 0 ), ColorEntry(
QColor(0xB2,0x68,0x18), 0, 0 ), // Green, Yellow
    ColorEntry( QColor(0x18,0x18,0xB2), 0, 0 ), ColorEntry(
QColor(0xB2,0x18,0xB2), 0, 0 ), // Blue, Magenta
    ColorEntry( QColor(0x18,0xB2,0xB2), 0, 0 ), ColorEntry(
QColor(0xB2,0xB2,0xB2), 0, 0 ), // Cyan, White
    // intensive
    ColorEntry( QColor(0x00,0x00,0x00), 0, 1 ), ColorEntry(
QColor(0xFF,0xFF,0xFF), 1, 0 ),
    ColorEntry( QColor(0x68,0x68,0x68), 0, 0 ), ColorEntry(
QColor(0xFF,0x54,0x54), 0, 0 ),
    ColorEntry( QColor(0x54,0xFF,0x54), 0, 0 ), ColorEntry(
QColor(0xFF,0xFF,0x54), 0, 0 ),
    ColorEntry( QColor(0x54,0x54,0xFF), 0, 0 ), ColorEntry(
QColor(0xFF,0x54,0xFF), 0, 0 ),
    ColorEntry( QColor(0x54,0xFF,0xFF), 0, 0 ), ColorEntry(
QColor(0xFF,0xFF,0xFF), 0, 0 )
};

const char* ColorScheme::colorNames[TABLE_COLORS] =
{
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

ColorSchemeManager* ColorSchemeManager::_instance = 0;

ColorScheme::ColorScheme()
{
    _table = 0;
}

ColorScheme::~ColorScheme()
{
    delete[] _table;
}

void ColorScheme::setDescription(const QString& description) { _description = description; }
QString ColorScheme::description() const { return _description; }

void ColorScheme::setName(const QString& name) { _name = name; }
QString ColorScheme::name() const { return _name; }

void ColorScheme::setColorTableEntry(int index , const ColorEntry& entry)
{
    Q_ASSERT( index >= 0 && index < TABLE_COLORS );

    if ( !_table )
        _table = new ColorEntry[TABLE_COLORS];
    
    _table[index] = entry; 
}
const ColorEntry* ColorScheme::colorTable() const
{
    if ( _table )
        return _table;
    else
        return defaultTable;
}
QColor ColorScheme::foregroundColor() const
{
    return colorTable()[0].color;
}
QColor ColorScheme::backgroundColor() const
{
    return colorTable()[1].color;
}
void ColorScheme::setOpacity(qreal opacity) { _opacity = opacity; }
qreal ColorScheme::opacity() const { return _opacity; }

void ColorScheme::read(KConfig& config)
{
    KConfigGroup configGroup = config.group("General");
    
    _description = configGroup.readEntry("Description",i18n("Un-named Color Scheme"));
    _opacity = configGroup.readEntry("Opacity",qreal(1.0));

    for (int i=0 ; i < TABLE_COLORS ; i++)
    {
        setColorTableEntry(i,readColorEntry(config,colorNameForIndex(i)));
    }
}
void ColorScheme::write(KConfig& config) const
{
    KConfigGroup configGroup = config.group("General");

    configGroup.writeEntry("Description",_description);
    configGroup.writeEntry("Opacity",_opacity);
    
    for (int i=0 ; i < TABLE_COLORS ; i++)
    {
        writeColorEntry(config,colorNameForIndex(i),colorTable()[i]);
    }
}

QString ColorScheme::colorNameForIndex(int index) 
{
    Q_ASSERT( index >= 0 && index < TABLE_COLORS );

    return QString(colorNames[index]);
}

ColorEntry ColorScheme::readColorEntry(KConfig& config , const QString& colorName)
{
    KConfigGroup configGroup(&config,colorName);
    
    ColorEntry entry;

    entry.color = configGroup.readEntry("Color",QColor());
    entry.transparent = configGroup.readEntry("Transparent",false);
    entry.bold = configGroup.readEntry("Bold",false);

    return entry;
}
void ColorScheme::writeColorEntry(KConfig& config , const QString& colorName, const ColorEntry& entry) const
{
    KConfigGroup configGroup(&config,colorName);

    configGroup.writeEntry("Color",entry.color);
    configGroup.writeEntry("Transparency",(bool)entry.transparent);
    configGroup.writeEntry("Bold",(bool)entry.bold);
}

KDE3ColorSchemeReader::KDE3ColorSchemeReader( QIODevice* device ) :
    _device(device)
{
}
ColorScheme* KDE3ColorSchemeReader::read() 
{
    Q_ASSERT( _device->openMode() == QIODevice::ReadOnly ||
              _device->openMode() == QIODevice::ReadWrite  );

    ColorScheme* scheme = new ColorScheme();

    QRegExp comment("#.*$");
    while ( !_device->atEnd() )
    {
        QString line(_device->readLine());
        line.replace(comment,QString::null);
        line = line.simplified();

        if ( line.isEmpty() )
            continue;

        if ( line.startsWith("color") )
        {
            readColorLine(line,scheme);
        }
        else if ( line.startsWith("title") )
        {
            readTitleLine(line,scheme);
        }
        else
        {
            qWarning() << "KDE 3 color scheme contains an unsupported feature, '" <<
                line << "'";
        } 
    }

    return scheme;
}
void KDE3ColorSchemeReader::readColorLine(const QString& line,ColorScheme* scheme)
{
    QStringList list = line.split(QChar(' '));

    Q_ASSERT(list.count() == 7);
    Q_ASSERT(list.first() == "color");
    
    int index = list[1].toInt();
    int red = list[2].toInt();
    int green = list[3].toInt();
    int blue = list[4].toInt();
    int transparent = list[5].toInt();
    int bold = list[6].toInt();

    const int MAX_COLOR_VALUE = 255;

    Q_ASSERT( index >= 0 && index < TABLE_COLORS );
    Q_ASSERT( red >= 0 && red <= MAX_COLOR_VALUE );
    Q_ASSERT( blue >= 0 && blue <= MAX_COLOR_VALUE );
    Q_ASSERT( green >= 0 && green <= MAX_COLOR_VALUE );
    Q_ASSERT( transparent == 0 || transparent == 1 );
    Q_ASSERT( bold == 0 || bold == 1 );

    ColorEntry entry;
    entry.color = QColor(red,green,blue);
    entry.transparent = ( transparent != 0 );
    entry.bold = ( bold != 0 );

    scheme->setColorTableEntry(index,entry);
}
void KDE3ColorSchemeReader::readTitleLine(const QString& line,ColorScheme* scheme)
{
    Q_ASSERT( line.startsWith("title") );

    int spacePos = line.indexOf(' ');
    Q_ASSERT( spacePos != -1 );

    scheme->setDescription(line.mid(spacePos+1));
}
ColorSchemeManager::ColorSchemeManager()
    : _haveLoadedAll(false)
{
}
ColorSchemeManager::~ColorSchemeManager()
{
    QHashIterator<QString,ColorScheme*> iter(_colorSchemes);
    while (iter.hasNext())
    {
        iter.next();
        delete iter.value();
    }
}
void ColorSchemeManager::loadAllColorSchemes()
{
    int success = 0;
    int failed = 0;

    QList<QString> nativeColorSchemes = listColorSchemes();
    
    QListIterator<QString> nativeIter(nativeColorSchemes);
    while ( nativeIter.hasNext() )
    {
        if ( loadColorScheme( nativeIter.next() ) )
            success++;
        else
            failed++;
    }

    QList<QString> kde3ColorSchemes = listKDE3ColorSchemes();
    QListIterator<QString> kde3Iter(kde3ColorSchemes);
    while ( kde3Iter.hasNext() )
    {
        if ( loadKDE3ColorScheme( kde3Iter.next() ) )
            success++;
        else
            failed++;
    }

    if ( success > 0 )
        qDebug() << "succeeded to load " << success << " color schemes.";
    if ( failed > 0 )
        qDebug() << "failed to load " << failed << " color schemes.";

    _haveLoadedAll = true;
}
QList<ColorScheme*> ColorSchemeManager::allColorSchemes()
{
    if ( !_haveLoadedAll )
    {
        loadAllColorSchemes();
    }

    return _colorSchemes.values();
}
bool ColorSchemeManager::loadKDE3ColorScheme(const QString& filePath)
{
    qDebug() << "loading KDE 3 format color scheme from " << filePath;

    QFile file(filePath);
    file.open(QIODevice::ReadOnly);

    KDE3ColorSchemeReader reader(&file);
    ColorScheme* scheme = reader.read();
    scheme->setName(QFileInfo(file).baseName());
    file.close();

    Q_ASSERT( !scheme->name().isEmpty() );

    qDebug() << "found KDE 3 format color scheme - " << scheme->name();
    
    QFileInfo info(filePath);

    if ( !_colorSchemes.contains(info.baseName()) )
    {
        qDebug() << "added color scheme - " << info.baseName();
        
        _colorSchemes.insert(scheme->name(),scheme);
    }
    else
    {
        qDebug() << "color scheme with name" << scheme->name() << "has already been" <<
            "found, ignoring.";
        delete scheme;
    }

    return true;
}
bool ColorSchemeManager::loadColorScheme(const QString& filePath)
{
    QFileInfo info(filePath);
    
    qDebug() << "loading KDE 4 native color scheme from " << filePath;
    KConfig config(filePath , KConfig::NoGlobals);
    ColorScheme* scheme = new ColorScheme();
    scheme->setName(info.baseName());
    scheme->read(config);
    
    Q_ASSERT( !scheme->name().isEmpty() );

    qDebug() << "found KDE 4 native color scheme - " << scheme->name();


    if ( !_colorSchemes.contains(info.baseName()) )
    {
        qDebug() << "added color scheme - " << info.baseName();

        _colorSchemes.insert(scheme->name(),scheme);
    }
    else
    {
        qDebug() << "color scheme with name" << scheme->name() << "has already been" <<
            "found, ignoring.";
        
        delete scheme;
    }

    return true; 
}
QList<QString> ColorSchemeManager::listKDE3ColorSchemes()
{
    return KGlobal::dirs()->findAllResources("data",
                                             "konsole/*.schema",
                                              KStandardDirs::NoDuplicates);
    
}
QList<QString> ColorSchemeManager::listColorSchemes()
{
    return KGlobal::dirs()->findAllResources("data",
                                             "konsole/*.colorscheme",
                                             KStandardDirs::NoDuplicates);
}
const ColorScheme ColorSchemeManager::_defaultColorScheme;
const ColorScheme* ColorSchemeManager::defaultColorScheme() const
{
    return &_defaultColorScheme;
}
const ColorScheme* ColorSchemeManager::findColorScheme(const QString& name) 
{
    qDebug() << "looking for color scheme - " << name;

    if ( name.isEmpty() )
        return defaultColorScheme();

    if ( _colorSchemes.contains(name) )
        return _colorSchemes[name];
    else
    {
        // look for this color scheme
        QString path = KStandardDirs::locate("data","konsole/"+name+".colorscheme"); 
        if ( !path.isEmpty() && loadColorScheme(path) )
        {
            return findColorScheme(name); 
        } 
        else // look for a KDE 3 format color scheme by this name
        {
            QString kde3path = KStandardDirs::locate("data","konsole/"+name+".schema");
            if (!kde3path.isEmpty() && loadKDE3ColorScheme(kde3path))
                return findColorScheme(name);
        }

        qDebug() << "Could not find color scheme - " << name;

        return 0; 
    }
}
ColorSchemeManager* ColorSchemeManager::instance()
{
    return _instance;
}
void ColorSchemeManager::setInstance(ColorSchemeManager* instance)
{
    _instance = instance;
}
