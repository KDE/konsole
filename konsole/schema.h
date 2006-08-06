/* schema.h
**
** Copyright (C) 1998-1999	by Lars Doelle <lars.doelle@on-line.de>
** Copyright (C) 2000		by Adriaan de Groot <groot@kde.org> 
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
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
** MA 02110-1301, USA.
*/

/*
** Lars' code has almost completely disappeared from this file.
** The new setup is more object-oriented and disposes of some
** nasty static deleters.
**
** Basically you want to create a ColorSchemaList and then call
** checkSchema's to get all the schema's available. Iterate through
** the items in the list (with a constiterator) and so whatever.
** The ColorSchemaList inherits protected from QList to prevent you
** from doing silly things to the elements of the list.
*/

#ifndef SCHEMA_include
#define SCHEMA_include

#include <qstring.h>
#include <qptrlist.h>

#include "TECommon.h"

#ifndef KONSOLEDEBUG
/*
** konsole has claimed debug area 1211. This isn't really the right place
** to define KONSOLEDEBUG but there is no "global.h" or central "konsole.h"
** file included by all parts of konsole, so this will have to do.
*/
#define KONSOLEDEBUG	kdDebug(1211)
#endif

class QDateTime;
class KConfig;

class ColorSchema
{
friend class ColorSchemaList; //only for resetting serial to one when deleting the list

public:
	/**
	* Create ColorSchema from the given pathname.
	* If the given pathname does not exist, a ColorSchema
	* with the same settings as the default schema is returned.
	*/
	ColorSchema(const QString& pathname);
    ~ColorSchema();
	/**
	* Construct a color schema from the given config file.
	* (This is different from the constructor with a pathname
	* because that reads the hackneyed schema file syntax and
	* this is a KDE config file)
	*/
	ColorSchema(KConfig&);

	/**
	* Constructor for the default schema (with no path).
	*/
	ColorSchema();

	QString relPath() const { return fRelPath; } ;

	/**
	* Check if the schema file whose pathname was given
	* to the constructor has changed since it was last read.
	*/
	bool hasSchemaFileChanged() const;

	/**
	* Actually read a schema file (using the path given
	* to the constructor of the ColorSchema).
	*/
	bool rereadSchemaFile();

	/**
	* Writes a ColorSchema to a config file with the
	* given name.
	*/
	void writeConfig(const QString& filename) const;

	/**
	* Returns the (non-i18n) name of the i'th color,
	* or QString::null if i is not a color name. This
	* should be used as a group name to store the
	* information about the i'th color.
	*/
	static QString colorName(int i);

	/**
	* Update the timestamp in the color schema indicating
	* when the schema's file whas last checked and read.
	*/
	void updateLastRead(const QDateTime& dt);


protected:
	/**
	* Clear a schema. Used by constructors to clean up the
	* data members before filling them.
	*/
	void clearSchema();

	/**
	* Set the data members' values to those of the
	* default schema.
	*/
	void setDefaultSchema();


	/**
	* Write a single ColorEntry to the config file
	* under the given name (ie. in the group name).
	*/
	void writeConfigColor(KConfig& c,
		const QString& name,
		const ColorEntry& e) const;
	/**
	* Read a single ColorEntry from the config file.
	*/
	void readConfigColor(KConfig& c,
		const QString& name,
		ColorEntry& e) ;

   public:
      int numb()                       {if (!m_fileRead) rereadSchemaFile();return m_numb;};
      const QString& title()           {if (!m_fileRead) rereadSchemaFile();return m_title;};
      const QString& imagePath()       {if (!m_fileRead) rereadSchemaFile();return m_imagePath;};
      int alignment()                  {if (!m_fileRead) rereadSchemaFile();return m_alignment;};
      const ColorEntry* table()        {if (!m_fileRead) rereadSchemaFile();return m_table;};
      bool useTransparency()           {if (!m_fileRead) rereadSchemaFile();return m_useTransparency;};
      double tr_x()                    {if (!m_fileRead) rereadSchemaFile();return m_tr_x;};
      int tr_r()                       {if (!m_fileRead) rereadSchemaFile();return m_tr_r;};
      int tr_g()                       {if (!m_fileRead) rereadSchemaFile();return m_tr_g;};
      int tr_b()                       {if (!m_fileRead) rereadSchemaFile();return m_tr_b;};
      QDateTime* getLastRead()   {return lastRead;};	// Time last checked for updates

   private:
      int        m_numb;
      int	     m_tr_r, m_tr_g, m_tr_b;
      int        m_alignment;
      QString    m_title;
      QString    m_imagePath;
      ColorEntry m_table[TABLE_COLORS];
      bool       m_useTransparency:1;
      bool       m_fileRead:1;
      double     m_tr_x;
      QString    fRelPath;	// File name of schema file
      QDateTime	*lastRead;	// Time last checked for updates
      static int	serial;		// Serial number so that every
      // ColorSchema has a unique number.
};

class ColorSchemaList : protected QPtrList<ColorSchema>
{
public:
	/**
	* The following functions are redeclared public since
	* they're needed, but we still want to inherit protected
	* from QPtrList to prevent unsightly -- and perhaps dangerous --
	* tampering with the ColorSchemaList.
	*/
	uint count() const { return QPtrList<ColorSchema>::count(); } ;
	const ColorSchema *at(unsigned int i)
		{ return QPtrList<ColorSchema>::at(i); } ;

   void sort() {QPtrList<ColorSchema>::sort();};

	ColorSchemaList();
   virtual ~ColorSchemaList();
	/**
	* Check if any new color schema files have been added since
	* the last time checkSchemas() was called. Any new files
	* are added to the list of schemas automatically.
	*
	* @return true if there were any changes to the list of schemas
	* @return false otherwise
	*/
	bool checkSchemas();

	/**
	* Returns the color schema read from the given path,
	* or NULL if no color schema with the given path is found.
	*/
	ColorSchema *find(const QString & path);
	/**
	* Returns the serial number of the color schema
	* with the given serial number, or NULL if there is none.
	*/
	ColorSchema *find(int);
	ColorSchema *findAny(const QString& path)
	{
		ColorSchema *p = find(path);
		if (p) return p;
		return defaultSchema;
	} ;
protected:
   virtual int compareItems(QPtrCollection::Item item1, QPtrCollection::Item item2);

private:
	/**
	* These next two functions are used internally by
	* checkSchemas. updateAllSchemaTimes sets the timestamp
	* on all the ColorSchema's whose config / schema file
	* can still be found, and deleteOldSchema's does the
	* actual removal of schema's without a config file.
	*/
	bool updateAllSchemaTimes(const QDateTime&);
	bool deleteOldSchemas(const QDateTime&);

	/**
	* This isn't really used, but it could be. A newly
	* constructed ColorSchemaList contains one element:
	* the defaultSchema, with serial number 0.
	*/
	ColorSchema *defaultSchema;
} ;

#endif
