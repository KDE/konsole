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

// Own
#include "ColorScheme.h"

// Qt
#include <QtGui/QBrush>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>

// KDE
#include <KColorScheme>
#include <KConfig>
#include <KLocale>
#include <KDebug>
#include <KConfigGroup>
#include <KStandardDirs>

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
const char* ColorScheme::translatedColorNames[TABLE_COLORS] =
{
    I18N_NOOP("Foreground"),
    I18N_NOOP("Background"),
    I18N_NOOP("Color 1"),
    I18N_NOOP("Color 2"),
    I18N_NOOP("Color 3"),
    I18N_NOOP("Color 4"),
    I18N_NOOP("Color 5"),
    I18N_NOOP("Color 6"),
    I18N_NOOP("Color 7"),
    I18N_NOOP("Color 8"),
    I18N_NOOP("Foreground (Intense)"),
    I18N_NOOP("Background (Intense)"),
    I18N_NOOP("Color 1 (Intense)"),
    I18N_NOOP("Color 2 (Intense)"),
    I18N_NOOP("Color 3 (Intense)"),
    I18N_NOOP("Color 4 (Intense)"),
    I18N_NOOP("Color 5 (Intense)"),
    I18N_NOOP("Color 6 (Intense)"),
    I18N_NOOP("Color 7 (Intense)"),
    I18N_NOOP("Color 8 (Intense)")
};

ColorScheme::ColorScheme()
{
    _table = 0;
    _randomTable = 0;
    _opacity = 1.0;
}
ColorScheme::ColorScheme(const ColorScheme& other)
      : _opacity(other._opacity)
       ,_table(0)
       ,_randomTable(0)
{
    setName(other.name());
    setDescription(other.description());

    if ( other._table != 0 )
    {
        for ( int i = 0 ; i < TABLE_COLORS ; i++ )
            setColorTableEntry(i,other._table[i]);
    }

    if ( other._randomTable != 0 )
    {
        for ( int i = 0 ; i < TABLE_COLORS ; i++ )
        {
            const RandomizationRange& range = other._randomTable[i];
            setRandomizationRange(i,range.hue,range.saturation,range.value);
        }
    }
}
ColorScheme::~ColorScheme()
{
    delete[] _table;
    delete[] _randomTable;
}

void ColorScheme::setDescription(const QString& description) { _description = description; }
QString ColorScheme::description() const { return _description; }

void ColorScheme::setName(const QString& name) { _name = name; }
QString ColorScheme::name() const { return _name; }

void ColorScheme::setColorTableEntry(int index , const ColorEntry& entry)
{
    Q_ASSERT( index >= 0 && index < TABLE_COLORS );

    if ( !_table ) 
    {
        _table = new ColorEntry[TABLE_COLORS];

        for (int i=0;i<TABLE_COLORS;i++)
            _table[i] = defaultTable[i];
    }
    
    _table[index] = entry; 
}
ColorEntry ColorScheme::colorEntry(int index , uint randomSeed) const
{
    Q_ASSERT( index >= 0 && index < TABLE_COLORS );

    if ( randomSeed != 0 )
        qsrand(randomSeed);

    ColorEntry entry = colorTable()[index];

  //  kDebug() << "Old color: " << entry.color;

   // kDebug() << "Random seed: " << randomSeed << "random table: " <<
   //     _randomTable << "isnull: " << ((_randomTable) ? _randomTable[index].isNull() : true);

    if ( randomSeed != 0 && 
        _randomTable != 0 && 
        !_randomTable[index].isNull() )
    {
        const RandomizationRange& range = _randomTable[index];
      

        int hueDifference = range.hue ? (qrand() % range.hue) - range.hue/2 : 0;
        int saturationDifference = range.saturation ? (qrand() % range.saturation) - range.saturation/2 : 0;
        int  valueDifference = range.value ? (qrand() % range.value) - range.value/2 : 0;

        QColor& color = entry.color;
      //  kDebug() << "Standard hue" << color.hue() << "saturation" << color.saturation() << "value" << color.value();

        int newHue = qAbs( (color.hue() + hueDifference) % MAX_HUE );
        int newValue = qMin( qAbs(color.value() + valueDifference) , 255 );
        int newSaturation = qMin( qAbs(color.saturation() + saturationDifference) , 255 );

    // kDebug() << "New hue: " << newHue << "New sat:" << newSaturation <<
    //        "New value:" << newValue;
        color.setHsv(newHue,newSaturation,newValue);
    }

  //  kDebug() << "New color: " << entry.color;

    return entry;
}
void ColorScheme::getColorTable(ColorEntry* table , uint randomSeed) const
{
    for ( int i = 0 ; i < TABLE_COLORS ; i++ )
        table[i] = colorEntry(i,randomSeed);
}
bool ColorScheme::randomizedBackgroundColor() const
{
    return _randomTable == 0 ? false : !_randomTable[1].isNull();
}
void ColorScheme::setRandomizedBackgroundColor(bool randomize)
{
    // the hue of the background colour is allowed to be randomly 
    // adjusted as much as possible.
    //
    // the value and saturation are left alone to maintain read-ability
    if ( randomize )
    {
        setRandomizationRange( 1 /* background color index */ , MAX_HUE , 255 , 0 ); 
    }
    else
    {
        if ( _randomTable )
            setRandomizationRange( 1 /* background color index */ , 0 , 0 , 0 );
    }
}

void ColorScheme::setRandomizationRange( int index , quint16 hue , quint8 saturation ,
                                         quint8 value )
{
    Q_ASSERT( hue <= MAX_HUE );
    Q_ASSERT( index >= 0 && index < TABLE_COLORS );

    if ( _randomTable == 0 )
        _randomTable = new RandomizationRange[TABLE_COLORS];

    _randomTable[index].hue = hue;
    _randomTable[index].value = value;
    _randomTable[index].saturation = saturation;
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
bool ColorScheme::hasDarkBackground() const
{
    // value can range from 0 - 255, with larger values indicating higher brightness.
    // so 127 is in the middle, anything less is deemed 'dark'
    return backgroundColor().value() < 127;
}
void ColorScheme::setOpacity(qreal opacity) { _opacity = opacity; }
qreal ColorScheme::opacity() const { return _opacity; }

void ColorScheme::read(KConfig& config)
{
    KConfigGroup configGroup = config.group("General");

    QString description = configGroup.readEntry("Description", I18N_NOOP("Un-named Color Scheme"));

    _description = i18n(description.toUtf8());
    _opacity = configGroup.readEntry("Opacity",qreal(1.0));

    for (int i=0 ; i < TABLE_COLORS ; i++)
    {
        readColorEntry(config,i);
    }
}
void ColorScheme::write(KConfig& config) const
{
    KConfigGroup configGroup = config.group("General");

    configGroup.writeEntry("Description",_description);
    configGroup.writeEntry("Opacity",_opacity);
    
    for (int i=0 ; i < TABLE_COLORS ; i++)
    {
        RandomizationRange random = _randomTable != 0 ? _randomTable[i] : RandomizationRange();
        writeColorEntry(config,colorNameForIndex(i),colorTable()[i],random);
    }
}

QString ColorScheme::colorNameForIndex(int index) 
{
    Q_ASSERT( index >= 0 && index < TABLE_COLORS );

    return QString(colorNames[index]);
}
QString ColorScheme::translatedColorNameForIndex(int index) 
{
    Q_ASSERT( index >= 0 && index < TABLE_COLORS );

    return i18n(translatedColorNames[index]);
}
void ColorScheme::readColorEntry(KConfig& config , int index)
{
    KConfigGroup configGroup(&config,colorNameForIndex(index));
    
    ColorEntry entry;

    entry.color = configGroup.readEntry("Color",QColor());
    entry.transparent = configGroup.readEntry("Transparent",false);
    entry.bold = configGroup.readEntry("Bold",false);

    quint16 hue = configGroup.readEntry("MaxRandomHue",0);
    quint8 value = configGroup.readEntry("MaxRandomValue",0);
    quint8 saturation = configGroup.readEntry("MaxRandomSaturation",0);

    setColorTableEntry( index , entry );

    if ( hue != 0 || value != 0 || saturation != 0 )
       setRandomizationRange( index , hue , saturation , value ); 
}
void ColorScheme::writeColorEntry(KConfig& config , const QString& colorName, const ColorEntry& entry , const RandomizationRange& random) const
{
    KConfigGroup configGroup(&config,colorName);

    configGroup.writeEntry("Color",entry.color);
    configGroup.writeEntry("Transparency",(bool)entry.transparent);
    configGroup.writeEntry("Bold",(bool)entry.bold);

    // record randomization if this color has randomization or 
    // if one of the keys already exists 
    if ( !random.isNull() || configGroup.hasKey("MaxRandomHue") )
    {
        configGroup.writeEntry("MaxRandomHue",(int)random.hue);
        configGroup.writeEntry("MaxRandomValue",(int)random.value);
        configGroup.writeEntry("MaxRandomSaturation",(int)random.saturation);
    }
}


// 
// Work In Progress - A color scheme for use on KDE setups for users
// with visual disabilities which means that they may have trouble
// reading text with the supplied color schemes.
//
// This color scheme uses only the 'safe' colors defined by the
// KColorScheme class.  
//
// A complication this introduces is that each color provided by 
// KColorScheme is defined as a 'background' or 'foreground' color.
// Only foreground colors are allowed to be used to render text and 
// only background colors are allowed to be used for backgrounds.
//
// The ColorEntry and TerminalDisplay classes do not currently
// support this restriction.  
//
// Requirements:
//  - A color scheme which uses only colors from the KColorScheme class
//  - Ability to restrict which colors the TerminalDisplay widget 
//    uses as foreground and background color
//  - Make use of KGlobalSettings::allowDefaultBackgroundImages() as
//    a hint to determine whether this accessible color scheme should 
//    be used by default.
//
//
// -- Robert Knight <robertknight@gmail.com> 21/07/2007
//
AccessibleColorScheme::AccessibleColorScheme()
    : ColorScheme()
{
    // basic attributes
    setName("accessible");
    setDescription(i18n("Accessible Color Scheme"));

    // setup colors
    const int ColorRoleCount = 8;

    const KColorScheme colorScheme(QPalette::Active);

    QBrush colors[ColorRoleCount] =
    {
        colorScheme.foreground( colorScheme.NormalText ),
        colorScheme.background( colorScheme.NormalBackground ),

        colorScheme.foreground( colorScheme.InactiveText ),
        colorScheme.foreground( colorScheme.ActiveText ),
        colorScheme.foreground( colorScheme.LinkText ),
        colorScheme.foreground( colorScheme.VisitedText ),
        colorScheme.foreground( colorScheme.NegativeText ),
        colorScheme.foreground( colorScheme.NeutralText )
    };

    for ( int i = 0 ; i < TABLE_COLORS ; i++ ) 
    {
        ColorEntry entry;
        entry.color = colors[ i % ColorRoleCount ].color();

        setColorTableEntry( i , entry ); 
    }   
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
        line.replace(comment,QString());
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
            kWarning() << "KDE 3 color scheme contains an unsupported feature, '" <<
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

    QString description = line.mid(spacePos+1);

    scheme->setDescription(i18n(description.toUtf8()));
}
ColorSchemeManager::ColorSchemeManager()
    : _haveLoadedAll(false)
{
}
ColorSchemeManager::~ColorSchemeManager()
{
    QHashIterator<QString,const ColorScheme*> iter(_colorSchemes);
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

   // if ( success > 0 )
   //     kDebug() << "succeeded to load " << success << " color schemes.";
    if ( failed > 0 )
        kDebug() << "failed to load " << failed << " color schemes.";

    _haveLoadedAll = true;
}
QList<const ColorScheme*> ColorSchemeManager::allColorSchemes()
{
    if ( !_haveLoadedAll )
    {
        loadAllColorSchemes();
    }

    return _colorSchemes.values();
}
bool ColorSchemeManager::loadKDE3ColorScheme(const QString& filePath)
{
    //kDebug() << "loading KDE 3 format color scheme from " << filePath;

    QFile file(filePath);
    if (!filePath.endsWith(".schema") || !file.open(QIODevice::ReadOnly))
        return false;

    KDE3ColorSchemeReader reader(&file);
    ColorScheme* scheme = reader.read();
    scheme->setName(QFileInfo(file).baseName());
    file.close();

    Q_ASSERT( !scheme->name().isEmpty() );

    //kDebug() << "found KDE 3 format color scheme - " << scheme->name();
    
    QFileInfo info(filePath);

    if ( !_colorSchemes.contains(info.baseName()) )
    {
        //kDebug() << "added color scheme - " << info.baseName();
        
        _colorSchemes.insert(scheme->name(),scheme);
    }
    else
    {
        kDebug() << "color scheme with name" << scheme->name() << "has already been" <<
            "found, ignoring.";
        delete scheme;
    }

    return true;
}
void ColorSchemeManager::addColorScheme(ColorScheme* scheme) 
{
    _colorSchemes.insert(scheme->name(),scheme);

    // save changes to disk
    QString path = KGlobal::dirs()->saveLocation("data","konsole/") + scheme->name() + ".colorscheme";
    KConfig config(path , KConfig::NoGlobals);

    scheme->write(config);
}
bool ColorSchemeManager::loadColorScheme(const QString& filePath)
{
    if ( !filePath.endsWith(".colorscheme") || !QFile::exists(filePath) )
        return false;

    QFileInfo info(filePath);
    
    KConfig config(filePath , KConfig::NoGlobals);
    ColorScheme* scheme = new ColorScheme();
    scheme->setName(info.baseName());
    scheme->read(config);

    if (scheme->name().isEmpty()) 
    {
        kWarning() << "Color scheme in" << filePath << "does not have a valid name and was not loaded.";
        delete scheme;
        return false;
    }    

    if ( !_colorSchemes.contains(info.baseName()) )
    {
        _colorSchemes.insert(scheme->name(),scheme);
    }
    else
    {
        kDebug() << "color scheme with name" << scheme->name() << "has already been" <<
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
bool ColorSchemeManager::deleteColorScheme(const QString& name)
{
    Q_ASSERT( _colorSchemes.contains(name) );

    // lookup the path and delete 
    QString path = findColorSchemePath(name);
    if ( QFile::remove(path) )
    {
        _colorSchemes.remove(name);
        return true;
    }
    else
    {
        kWarning() << "Failed to remove color scheme -" << path;
        return false;
    }
}
QString ColorSchemeManager::findColorSchemePath(const QString& name) const
{
    QString path = KStandardDirs::locate("data","konsole/"+name+".colorscheme");

    if ( !path.isEmpty() )
       return path; 

    path = KStandardDirs::locate("data","konsole/"+name+".schema");

    return path;
}
const ColorScheme* ColorSchemeManager::findColorScheme(const QString& name) 
{
    //kDebug() << "looking for color scheme - " << name;

    if ( name.isEmpty() )
        return defaultColorScheme();

    if ( _colorSchemes.contains(name) )
        return _colorSchemes[name];
    else
    {
        // look for this color scheme
        QString path = findColorSchemePath(name); 
        if ( !path.isEmpty() && loadColorScheme(path) )
        {
            return findColorScheme(name); 
        } 
        else 
        {
            if (!path.isEmpty() && loadKDE3ColorScheme(path))
                return findColorScheme(name);
        }

        kDebug() << "Could not find color scheme - " << name;

        return 0; 
    }
}
K_GLOBAL_STATIC( ColorSchemeManager , theColorSchemeManager )
ColorSchemeManager* ColorSchemeManager::instance()
{
    return theColorSchemeManager;
}
