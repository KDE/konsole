/* schema.C
**
** Copyright (C) 1998-1999	by Lars Doelle
** Copyright (C) 2000		by Adriaan de Groot
**
** A file that defines the objects for storing color schema's
** in konsole. This file is part of the KDE project, see
**	http://www.kde.org/
** for more information.
*/
 
/*
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program in a file called COPYING; if not, write to
** the Free Software Foundation, Inc., 675 Mass Ave, Cambridge,
** MA 02139, USA.
*/


/* Lars' comments were the following. They are not true anymore: */

/*
**  This is new stuff, so no docs yet.
**
**  The identifier is the path. `numb' is guarantied to range from 0 to
**  count-1. Note when reloading the path can be assumed to still identify
**  a know schema, while the `numb' may vary.
*/


/*
** NUMB IS NOT GUARANTEED ANYMORE. Since schema's may be created and
** destroyed as the list is checked, there may be gaps in the serial
** numbers and numb may be .. whatever. The default schema always has
** number 0, the rest may vary. Use find(int) to find a schema with
** a particular number, but remember that find may return NULL.
*/

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include "schema.h"
#include "kapp.h"

#include <qfile.h>
#include <qdir.h>
#include <qdatetime.h> 
#include <kstddirs.h>
#include <kglobal.h>
#include <klocale.h>
#include <kconfig.h>
#include <kdebug.h>


typedef QListIterator<ColorSchema> ColorSchemaListIterator;


// Number all the new color schema's (non-default) from 1.
//
//
int ColorSchema::serial = 1;

// Names of all the colors, to be used as group names
// in the config files. These do not have to be i18n'ed.
//
//
static const char *colornames[TABLE_COLORS] =
{
	"fgnormal",
	"bgnormal",
	"bg0",
	"bg1",
	"bg2",
	"bg3",
	"bg4",
	"bg5",
	"bg6",
	"bg7",
	"fgintense",
	"bgintense",
	"bg0i",
	"bg1i",
	"bg2i",
	"bg3i",
	"bg4i",
	"bg5i",
	"bg6i",
	"bg7i"
} ;


static const ColorEntry default_table[TABLE_COLORS] =
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
QColor(0xB2,0x18,0xB2), 0, 0 ), // Blue,  Magenta
    ColorEntry( QColor(0x18,0xB2,0xB2), 0, 0 ), ColorEntry( 
QColor(0xB2,0xB2,0xB2), 0, 0 ), // Cyan,  White
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

ColorSchema::ColorSchema(const QString& pathname) :
	fPath(pathname),
	lastRead(new QDateTime())
{
	if (pathname.isNull() || !QFile::exists(pathname))
	{
		fPath = QString::null;
		setDefaultSchema();
	}
	else
	{
		clearSchema();
		(void) rereadSchemaFile();
	}

	numb = serial++;
}

ColorSchema::ColorSchema() :
	fPath(QString::null),
	lastRead(0L)
{
	setDefaultSchema();
	numb = 0;
}



void ColorSchema::clearSchema()
{
	int i;

	for (i = 0; i < TABLE_COLORS; i++)
	{
		table[i].color       = QColor(0,0,0);
		table[i].transparent = 0;
		table[i].bold        = 0;
	}
	title     = i18n("[no title]");
	imagepath = "";
	alignment = 1;
	usetransparency = false;
	tr_x = 0.0;
	tr_r = 0;
	tr_g = 0;
	tr_b = 0;
}

void ColorSchema::setDefaultSchema()
{
  numb = 0;
  title = i18n("Konsole Default");
  imagepath = ""; // background pixmap
  alignment = 1;  // none
  usetransparency = false; // not use pseudo-transparency by default
  tr_r = tr_g = tr_b = 0; // just to be on the safe side
  tr_x = 0.0;
  for (int i = 0; i < TABLE_COLORS; i++)
  {
    table[i] = default_table[i];
  }
}

/* static */ QString ColorSchema::colorName(int i)
{
	if ((i<0) || (i>=TABLE_COLORS))
	{
		kdWarning() << "Request for color name "
			<< i
			<< " out of range."
			<< endl;
		return QString::null;
	}

	return QString(colornames[i]);
}

void ColorSchema::writeConfigColor(KConfig& c,
	const QString& name,
	const ColorEntry& e) const
{
	KConfigGroupSaver(&c,name);
	c.setGroup(name);
	c.writeEntry("Color",e.color);
	c.writeEntry("Transparency",(bool) e.transparent);
	c.writeEntry("Bold",(bool) e.bold);
}

void ColorSchema::readConfigColor(KConfig& c,
	const QString& name,
	ColorEntry& e)
{
	KConfigGroupSaver(&c,name);
	c.setGroup(name);

	e.color = c.readColorEntry("Color");
	e.transparent = c.readBoolEntry("Transparent",false);
	e.bold = c.readBoolEntry("Bold",false);
}


void ColorSchema::writeConfig(const QString& path) const
{
	KONSOLEDEBUG << "Writing schema "
		<< fPath
		<< " to file "
		<< path
		<< endl;
		
	KConfig c(path,false,false);

	c.setGroup("SchemaGeneral");
	c.writeEntry("Title",title);
	c.writeEntry("ImagePath",imagepath);
	c.writeEntry("ImageAlignment",alignment);
	c.writeEntry("UseTransparency",usetransparency);

	c.writeEntry("TransparentR",tr_r);
	c.writeEntry("TransparentG",tr_g);
	c.writeEntry("TransparentB",tr_b);
	c.writeEntry("TransparentX",tr_x);

	for (int i=0; i < TABLE_COLORS; i++)
	{
		writeConfigColor(c,colorName(i),table[i]);
	}
}

ColorSchema::ColorSchema(KConfig& c) :
	fPath(QString::null),
	lastRead(0L)
{
	clearSchema();

	c.setGroup("SchemaGeneral");

	title = c.readEntry("Title",i18n("[no title]"));
	imagepath = c.readEntry("ImagePath",QString::null);
	alignment = c.readNumEntry("ImageAlignment",1);
	usetransparency = c.readBoolEntry("UseTransparency",false);

	tr_r = c.readNumEntry("TransparentR",0);
	tr_g = c.readNumEntry("TransparentG",0);
	tr_b = c.readNumEntry("TransparentB",0);
	tr_x = c.readDoubleNumEntry("TransparentX",0.0);

	for (int i=0; i < TABLE_COLORS; i++)
	{
		readConfigColor(c,colorName(i),table[i]);
	}

	numb = serial++;
}

bool ColorSchema::rereadSchemaFile()
{
	if (fPath.isNull()) return false;

	KONSOLEDEBUG << "Rereading schema file " << fPath << endl;

	FILE *sysin = fopen(QFile::encodeName(fPath),"r");
	if (!sysin)
	{
		int e = errno;

		kdWarning() << "Schema file "
			<< fPath
			<< " could not be opened ("
			<< strerror(e)
			<< ")"
			<< endl;
		return false;
	}

	char line[100];

	*lastRead = QDateTime::currentDateTime();

  while (fscanf(sysin,"%80[^\n]\n",line) > 0)
  {
    if (strlen(line) > 5)
    {
      if (!strncmp(line,"title",5))
      {
        title = i18n(line+6);
      }
      if (!strncmp(line,"image",5))
      { char rend[100], path[100]; int attr = 1;
        if (sscanf(line,"image %s %s",rend,path) != 2)
          continue;
        if (!strcmp(rend,"tile"  )) attr = 2; else
        if (!strcmp(rend,"center")) attr = 3; else
        if (!strcmp(rend,"full"  )) attr = 4; else
          continue;

        imagepath = locate("wallpaper", path);
        alignment = attr;
      }
      if (!strncmp(line,"transparency",12))
      { float rx;
        int rr, rg, rb;

	// Transparency needs 4 parameters: fade strength and the 3
	// components of the fade color.
        if (sscanf(line,"transparency %g %d %d %d",&rx,&rr,&rg,&rb) != 4)
          continue;
	usetransparency=true;
	tr_x=rx;
	tr_r=rr;
	tr_g=rg;
	tr_b=rb;
      }
      if (!strncmp(line,"color",5))
      { int fi,cr,cg,cb,tr,bo;
        if(sscanf(line,"color %d %d %d %d %d %d",&fi,&cr,&cg,&cb,&tr,&bo) != 6)
          continue;
        if (!(0 <= fi && fi <= TABLE_COLORS)) continue;
        if (!(0 <= cr && cr <= 255         )) continue;
        if (!(0 <= cg && cg <= 255         )) continue;
        if (!(0 <= cb && cb <= 255         )) continue;
        if (!(0 <= tr && tr <= 1           )) continue;
        if (!(0 <= bo && bo <= 1           )) continue;
        table[fi].color       = QColor(cr,cg,cb);
        table[fi].transparent = tr;
        table[fi].bold        = bo;
      }
      if (!strncmp(line,"sysfg",5))
      { int fi,tr,bo;
        if(sscanf(line,"sysfg %d %d %d",&fi,&tr,&bo) != 3)
          continue;
        if (!(0 <= fi && fi <= TABLE_COLORS)) continue;
        if (!(0 <= tr && tr <= 1           )) continue;
        if (!(0 <= bo && bo <= 1           )) continue;
        table[fi].color       = kapp->palette().normal().text();
        table[fi].transparent = tr;
        table[fi].bold        = bo;
      }
      if (!strncmp(line,"sysbg",5))
      { int fi,tr,bo;
        if(sscanf(line,"sysbg %d %d %d",&fi,&tr,&bo) != 3)
          continue;
        if (!(0 <= fi && fi <= TABLE_COLORS)) continue;
        if (!(0 <= tr && tr <= 1           )) continue;
        if (!(0 <= bo && bo <= 1           )) continue;
        table[fi].color       = kapp->palette().normal().base();
        table[fi].transparent = tr;
        table[fi].bold        = bo;
      }
    }
  }
  fclose(sysin);

	return true;
}

bool ColorSchema::hasSchemaFileChanged() const
{
	KONSOLEDEBUG << "Checking schema file "
		<< fPath
		<< endl;

	// The default color schema never changes.
	//
	//
	if (fPath.isNull()) return false;

	QFileInfo i(fPath);

	if (i.exists())
	{
		QDateTime written = i.lastModified();

		if (written > (*lastRead))
		{
			KONSOLEDEBUG << "Schema file was modified "
				<< written.toString()
				<< endl;

			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		kdWarning() << "Schema file no longer exists."
			<< endl;
		return false;
	}
}

void ColorSchema::updateLastRead(const QDateTime& dt)
{
	if (lastRead)
	{
		*lastRead=dt;
	}
}


ColorSchemaList::ColorSchemaList() :
	QList<ColorSchema> ()
{
	KONSOLEDEBUG << "Got new color list" << endl;

	defaultSchema = new ColorSchema();
	append(defaultSchema);
	setAutoDelete(true);
}



ColorSchema *ColorSchemaList::find(const QString& path)
{
	KONSOLEDEBUG << "Looking for schema " << path << endl;

	ColorSchemaListIterator it(*this);
	ColorSchema *c;

	while ((c=it.current()))
	{
		if ((*it)->path() == path) return *it;
		++it;
	}

	return 0;
}

ColorSchema *ColorSchemaList::find(int i)
{
	KONSOLEDEBUG << "Looking for schema number " << i << endl;

	ColorSchemaListIterator it(*this);
	ColorSchema *c;

	while ((c=it.current()))
	{
		if ((*it)->numb == i) return *it;
		++it;
	}

	return 0;
}

bool ColorSchemaList::updateAllSchemaTimes(const QDateTime& now)
{
	KONSOLEDEBUG << "Updating time stamps" << endl;

	QStringList list = KGlobal::dirs()->
		findAllResources("appdata", "*.schema");
	QStringList::ConstIterator it;
	bool r = false;

	for (it=list.begin(); it!=list.end(); ++it)
	{
		ColorSchema *sc = find(*it);

		if (!sc)
		{
			KONSOLEDEBUG << "Found new schema " << *it << endl;

			ColorSchema *newSchema = new ColorSchema(*it);
			if (newSchema) 
			{
				append(newSchema);
				r=true;
			}
		}
		else
		{
			if (sc->hasSchemaFileChanged())
			{
				sc->rereadSchemaFile();
			}
			else
			{
				sc->updateLastRead(now);
			}
		}
	}

	return r;
}

bool ColorSchemaList::deleteOldSchemas(const QDateTime& now)
{
	KONSOLEDEBUG << "Checking for vanished schemas" << endl;

	ColorSchemaListIterator it(*this);
	ColorSchema *p ;
	bool r = false;

	while ((p=it.current()))
	{
		if ((p->lastRead) && (*(p->lastRead)) < now)
		{
			KONSOLEDEBUG << "Found deleted schema "
				<< p->path()
				<< endl;
			++it;
			remove(p);
			r=true;
			if (!it.current())
			{
				break;
			}
		}
		else
		{
			++it;
		}
	}

	return r;
}


bool ColorSchemaList::checkSchemas()
{
	KONSOLEDEBUG << "Checking for new schemas" << endl;

	bool r = false;	// Any new schema's found?

	// All schemas whose schema files can still be found
	// will have their lastRead timestamps updated to
	// now.
	// 
	//
	QDateTime now = QDateTime::currentDateTime();
	

	r = updateAllSchemaTimes(now);
	r = r || deleteOldSchemas(now);

	return r;
}


