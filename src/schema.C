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
**  The identifier is the path. `m_numb' is guarantied to range from 0 to
**  count-1. Note when reloading the path can be assumed to still identify
**  a know schema, while the `m_numb' may vary.
*/


/*
** NUMB IS NOT GUARANTEED ANYMORE. Since schema's may be created and
** destroyed as the list is checked, there may be gaps in the serial
** m_numbers and m_numb may be .. whatever. The default schema always has
** m_number 0, the rest may vary. Use find(int) to find a schema with
** a particular m_number, but remember that find may return NULL.
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

ColorSchema::ColorSchema(const QString& pathname)
:m_fileRead(false)
,lastRead(new QDateTime())
{
    fPath = locate("appdata", pathname);
	if (fPath.isEmpty() || !QFile::exists(fPath))
	{
		fPath = QString::null;
		setDefaultSchema();
	}
	else
	{
		clearSchema();
		(void) rereadSchemaFile();
	}

	m_numb = serial++;
}

ColorSchema::ColorSchema()
:m_fileRead(false)
,fPath(QString::null)
,lastRead(0L)
{
	setDefaultSchema();
	m_numb = 0;
}

ColorSchema::ColorSchema(KConfig& c)
:m_fileRead(false)
,fPath(QString::null)
,lastRead(0L)
{
	clearSchema();

	c.setGroup("SchemaGeneral");

	m_title = c.readEntry("Title",i18n("[no title]"));
	m_imagePath = c.readEntry("ImagePath",QString::null);
	m_alignment = c.readNumEntry("ImageAlignment",1);
	m_useTransparency = c.readBoolEntry("UseTransparency",false);

	m_tr_r = c.readNumEntry("TransparentR",0);
	m_tr_g = c.readNumEntry("TransparentG",0);
	m_tr_b = c.readNumEntry("TransparentB",0);
	m_tr_x = c.readDoubleNumEntry("TransparentX",0.0);

	for (int i=0; i < TABLE_COLORS; i++)
	{
		readConfigColor(c,colorName(i),m_table[i]);
	}

	m_numb = serial++;
}



void ColorSchema::clearSchema()
{
	int i;

	for (i = 0; i < TABLE_COLORS; i++)
	{
		m_table[i].color       = QColor(0,0,0);
		m_table[i].transparent = 0;
		m_table[i].bold        = 0;
	}
	m_title     = i18n("[no title]");
	m_imagePath = "";
	m_alignment = 1;
	m_useTransparency = false;
	m_tr_x = 0.0;
	m_tr_r = 0;
	m_tr_g = 0;
	m_tr_b = 0;
}

void ColorSchema::setDefaultSchema()
{
  m_numb = 0;
  m_title = i18n("Konsole Default");
  m_imagePath = ""; // background pixmap
  m_alignment = 1;  // none
  m_useTransparency = false; // not use pseudo-transparency by default
  m_tr_r = m_tr_g = m_tr_b = 0; // just to be on the safe side
  m_tr_x = 0.0;
  for (int i = 0; i < TABLE_COLORS; i++)
  {
    m_table[i] = default_table[i];
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
//	KONSOLEDEBUG << "Writing schema " << fPath << " to file " << path << endl;
		
	KConfig c(path,false,false);

	c.setGroup("SchemaGeneral");
	c.writeEntry("Title",m_title);
	c.writeEntry("ImagePath",m_imagePath);
	c.writeEntry("ImageAlignment",m_alignment);
	c.writeEntry("UseTransparency",m_useTransparency);

	c.writeEntry("TransparentR",m_tr_r);
	c.writeEntry("TransparentG",m_tr_g);
	c.writeEntry("TransparentB",m_tr_b);
	c.writeEntry("TransparentX",m_tr_x);

	for (int i=0; i < TABLE_COLORS; i++)
	{
		writeConfigColor(c,colorName(i),m_table[i]);
	}
}

bool ColorSchema::rereadSchemaFile()
{
	if (fPath.isEmpty()) return false;

	//KONSOLEDEBUG << "Rereading schema file " << fPath << endl;


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
        m_title = i18n(line+6);
      }
      if (!strncmp(line,"image",5))
      { char rend[100], path[100]; int attr = 1;
        if (sscanf(line,"image %s %s",rend,path) != 2)
          continue;
        if (!strcmp(rend,"tile"  )) attr = 2; else
        if (!strcmp(rend,"center")) attr = 3; else
        if (!strcmp(rend,"full"  )) attr = 4; else
          continue;

        m_imagePath = locate("wallpaper", path);
        m_alignment = attr;
      }
      if (!strncmp(line,"transparency",12))
      { float rx;
        int rr, rg, rb;

	// Transparency needs 4 parameters: fade strength and the 3
	// components of the fade color.
        if (sscanf(line,"transparency %g %d %d %d",&rx,&rr,&rg,&rb) != 4)
          continue;
	m_useTransparency=true;
	m_tr_x=rx;
	m_tr_r=rr;
	m_tr_g=rg;
	m_tr_b=rb;
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
        m_table[fi].color       = QColor(cr,cg,cb);
        m_table[fi].transparent = tr;
        m_table[fi].bold        = bo;
      }
      if (!strncmp(line,"sysfg",5))
      { int fi,tr,bo;
        if(sscanf(line,"sysfg %d %d %d",&fi,&tr,&bo) != 3)
          continue;
        if (!(0 <= fi && fi <= TABLE_COLORS)) continue;
        if (!(0 <= tr && tr <= 1           )) continue;
        if (!(0 <= bo && bo <= 1           )) continue;
        m_table[fi].color       = kapp->palette().normal().text();
        m_table[fi].transparent = tr;
        m_table[fi].bold        = bo;
      }
      if (!strncmp(line,"sysbg",5))
      { int fi,tr,bo;
        if(sscanf(line,"sysbg %d %d %d",&fi,&tr,&bo) != 3)
          continue;
        if (!(0 <= fi && fi <= TABLE_COLORS)) continue;
        if (!(0 <= tr && tr <= 1           )) continue;
        if (!(0 <= bo && bo <= 1           )) continue;
        m_table[fi].color       = kapp->palette().normal().base();
        m_table[fi].transparent = tr;
        m_table[fi].bold        = bo;
      }
    }
  }
  fclose(sysin);
  m_fileRead=true;
	return true;
}

bool ColorSchema::hasSchemaFileChanged() const
{
	//KONSOLEDEBUG << "Checking schema file " << fPath << endl;

	// The default color schema never changes.
	//
	//
	if (fPath.isEmpty()) return false;

	QFileInfo i(fPath);

	if (i.exists())
	{
		QDateTime written = i.lastModified();

		if (written > (*lastRead))
		{
//			KONSOLEDEBUG << "Schema file was modified " << written.toString() << endl;

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
//	KONSOLEDEBUG << "Got new color list" << endl;

	defaultSchema = new ColorSchema();
	append(defaultSchema);
	setAutoDelete(true);
}

ColorSchemaList::~ColorSchemaList()
{
   ColorSchema::serial=1;
}


//now it works also if you give only the filename without path
ColorSchema *ColorSchemaList::find(const QString& path)
{
   if (path.isEmpty()) 
      return find(0);
	//KONSOLEDEBUG << "Looking for schema " << path << endl;
   //kdDebug(1211)<<"ColorSchema::find() count()=="<<count()<<endl;
	ColorSchemaListIterator it(*this);
	ColorSchema *c;
   bool pathIsOnlyFileName=(!path.contains("/"));

	while ((c=it.current()))
	{
      if ((pathIsOnlyFileName) && ((*it)->path().contains(path))) return *it;
      else if ((*it)->path() == path) return *it;
		++it;
	}

   //list is empty except the default schema
   if (count()==1)
   {
      //kdDebug(1211)<<"ColorSchema::find() empty"<<endl;
      ColorSchema *newSchema = new ColorSchema(path);
      if (newSchema)
         append(newSchema);
      return newSchema;
   };
   return 0;
}

ColorSchema *ColorSchemaList::find(int i)
{
	//KONSOLEDEBUG << "Looking for schema m_number " << i << endl;

	ColorSchemaListIterator it(*this);
	ColorSchema *c;

	while ((c=it.current()))
	{
		if ((*it)->numb() == i) return *it;
		++it;
	}

	return 0;
}

bool ColorSchemaList::updateAllSchemaTimes(const QDateTime& now)
{
//	KONSOLEDEBUG << "Updating time stamps" << endl;

	QStringList list = KGlobal::dirs()->
		findAllResources("appdata", "*.schema");
	QStringList::ConstIterator it;
	bool r = false;

	for (it=list.begin(); it!=list.end(); ++it)
	{
		ColorSchema *sc = find(*it);

		if (!sc)
		{
//			KONSOLEDEBUG << "Found new schema " << *it << endl;

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
   sort();
	return r;
}

bool ColorSchemaList::deleteOldSchemas(const QDateTime& now)
{
//	KONSOLEDEBUG << "Checking for vanished schemas" << endl;

	ColorSchemaListIterator it(*this);
	ColorSchema *p ;
	bool r = false;

	while ((p=it.current()))
	{
		if ((p->getLastRead()) && (*(p->getLastRead())) < now)
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
//	KONSOLEDEBUG << "Checking for new schemas" << endl;

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

int ColorSchemaList::compareItems(QCollection::Item item1, QCollection::Item item2)
{
   ColorSchema* schema1=(ColorSchema*)item1;
   ColorSchema* schema2=(ColorSchema*)item2;
   return -1*QString::compare(schema1->title(),schema2->title());
};


