/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

#ifndef TEHISTORY_H
#define TEHISTORY_H

#include <qcstring.h>
#include <qptrvector.h>
#include <qbitarray.h>

#include <ktempfile.h>

#include "TECommon.h"

#if 1
/*
   An extendable tmpfile(1) based buffer.
*/

class HistoryFile
{
public:
  HistoryFile();
  virtual ~HistoryFile();

  virtual void add(const unsigned char* bytes, int len);
  virtual void get(unsigned char* bytes, int len, int loc);
  virtual int  len();

private:
  int  ion;
  int  length;
  KTempFile tmpFile;
};
#endif

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Abstract base class for file and buffer versions
//////////////////////////////////////////////////////////////////////
class HistoryType;

class HistoryScroll
{
public:
  HistoryScroll(HistoryType*);
 virtual ~HistoryScroll();

  virtual bool hasScroll();

  // access to history
  virtual int  getLines() = 0;
  virtual int  getLineLen(int lineno) = 0;
  virtual void getCells(int lineno, int colno, int count, ca res[]) = 0;
  virtual bool isWrappedLine(int lineno) = 0;

  // backward compatibility (obsolete)
  ca   getCell(int lineno, int colno) { ca res; getCells(lineno,colno,1,&res); return res; }

  // adding lines.
  virtual void addCells(ca a[], int count) = 0;
  virtual void addLine(bool previousWrapped=false) = 0;

  const HistoryType& getType() { return *m_histType; }

protected:
  HistoryType* m_histType;

};

#if 1

//////////////////////////////////////////////////////////////////////
// File-based history (e.g. file log, no limitation in length)
//////////////////////////////////////////////////////////////////////

class HistoryScrollFile : public HistoryScroll
{
public:
  HistoryScrollFile(const QString &logFileName);
  virtual ~HistoryScrollFile();

  virtual int  getLines();
  virtual int  getLineLen(int lineno);
  virtual void getCells(int lineno, int colno, int count, ca res[]);
  virtual bool isWrappedLine(int lineno);

  virtual void addCells(ca a[], int count);
  virtual void addLine(bool previousWrapped=false);

private:
  int startOfLine(int lineno);

  QString m_logFileName;
  HistoryFile index; // lines Row(int)
  HistoryFile cells; // text  Row(ca)
  HistoryFile lineflags; // flags Row(unsigned char)
};


//////////////////////////////////////////////////////////////////////
// Buffer-based history (limited to a fixed nb of lines)
//////////////////////////////////////////////////////////////////////
class HistoryScrollBuffer : public HistoryScroll
{
public:
  typedef QMemArray<ca> histline;

  HistoryScrollBuffer(unsigned int maxNbLines = 1000);
  virtual ~HistoryScrollBuffer();

  virtual int  getLines();
  virtual int  getLineLen(int lineno);
  virtual void getCells(int lineno, int colno, int count, ca res[]);
  virtual bool isWrappedLine(int lineno);

  virtual void addCells(ca a[], int count);
  virtual void addLine(bool previousWrapped=false);

  void setMaxNbLines(unsigned int nbLines);
  unsigned int maxNbLines() { return m_maxNbLines; }
  

private:
  int adjustLineNb(int lineno);

  QPtrVector<histline> m_histBuffer;
  QBitArray m_wrappedLine;
  unsigned int m_maxNbLines;
  unsigned int m_nbLines;
  unsigned int m_arrayIndex;

};

#endif

//////////////////////////////////////////////////////////////////////
// Nothing-based history (no history :-)
//////////////////////////////////////////////////////////////////////
class HistoryScrollNone : public HistoryScroll
{
public:
  HistoryScrollNone();
  virtual ~HistoryScrollNone();

  virtual bool hasScroll();

  virtual int  getLines();
  virtual int  getLineLen(int lineno);
  virtual void getCells(int lineno, int colno, int count, ca res[]);
  virtual bool isWrappedLine(int lineno);

  virtual void addCells(ca a[], int count);
  virtual void addLine(bool previousWrapped=false);
};

//////////////////////////////////////////////////////////////////////
// BlockArray-based history
//////////////////////////////////////////////////////////////////////
#include "BlockArray.h"
#include <qintdict.h>
class HistoryScrollBlockArray : public HistoryScroll
{
public:
  HistoryScrollBlockArray(size_t size);
  virtual ~HistoryScrollBlockArray();

  virtual int  getLines();
  virtual int  getLineLen(int lineno);
  virtual void getCells(int lineno, int colno, int count, ca res[]);
  virtual bool isWrappedLine(int lineno);

  virtual void addCells(ca a[], int count);
  virtual void addLine(bool previousWrapped=false);

protected:
  BlockArray m_blockArray;
  QIntDict<size_t> m_lineLengths;
};

//////////////////////////////////////////////////////////////////////
// History type
//////////////////////////////////////////////////////////////////////

class HistoryType
{
public:
  HistoryType();
  virtual ~HistoryType();

  virtual bool isOn()                const = 0;
  virtual unsigned int getSize()     const = 0;

  virtual HistoryScroll* getScroll(HistoryScroll *) const = 0;
};

class HistoryTypeNone : public HistoryType
{
public:
  HistoryTypeNone();

  virtual bool isOn() const;
  virtual unsigned int getSize() const;

  virtual HistoryScroll* getScroll(HistoryScroll *) const;
};

class HistoryTypeBlockArray : public HistoryType
{
public:
  HistoryTypeBlockArray(size_t size);
  
  virtual bool isOn() const;
  virtual unsigned int getSize() const;

  virtual HistoryScroll* getScroll(HistoryScroll *) const;

protected:
  size_t m_size;
};

#if 1 // Disabled for now 
class HistoryTypeFile : public HistoryType
{
public:
  HistoryTypeFile(const QString& fileName=QString::null);

  virtual bool isOn() const;
  virtual const QString& getFileName() const;
  virtual unsigned int getSize() const;

  virtual HistoryScroll* getScroll(HistoryScroll *) const;

protected:
  QString m_fileName;
};


class HistoryTypeBuffer : public HistoryType
{
public:
  HistoryTypeBuffer(unsigned int nbLines);
  
  virtual bool isOn() const;
  virtual unsigned int getSize() const;

  virtual HistoryScroll* getScroll(HistoryScroll *) const;

protected:
  unsigned int m_nbLines;
};

#endif

#endif // TEHISTORY_H
