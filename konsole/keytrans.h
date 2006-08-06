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

#ifndef KEYTRANS_H
#define KEYTRANS_H

#include <qstring.h>
#include <qptrlist.h>
#include <qiodevice.h>

#define BITS_NewLine    0
#define BITS_BsHack     1
#define BITS_Ansi       2
#define BITS_AppCuKeys  3
#define BITS_Control    4
#define BITS_Shift      5
#define BITS_Alt        6
#define BITS_AppScreen  7
#define BITS_COUNT      8
#define BITS_AnyMod     9

#define CMD_none          -1
#define CMD_send           0
#define CMD_scrollPageUp   2
#define CMD_scrollPageDown 3
#define CMD_scrollLineUp   4
#define CMD_scrollLineDown 5
#define CMD_scrollLock     6

#define BITS(x,v) ((((v)!=0)<<(x)))

class KeytabReader;

class KeyTrans
{
   friend class KeytabReader;
   public:
      static KeyTrans* find(int numb);
      static KeyTrans* find(const QString &id);
      static int count();
      static void loadAll();

      KeyTrans(const QString& p);
      ~KeyTrans();
      bool findEntry(int key, int bits, int* cmd, const char** txt, int* len,
                     bool* metaspecified);
      const QString& hdr()         {if (!m_fileRead) readConfig(); return m_hdr;}
      int numb()                   {return m_numb;}
      const QString& id() { return m_id;}

      class KeyEntry
      {
         public:
            KeyEntry(int ref, int key, int bits, int mask, int cmd, QString txt);
            ~KeyEntry();
            bool matches(int key, int bits, int mask);
            bool metaspecified(void);
            bool anymodspecified(void);
            QString text();
            int ref;
         private:
            int     key;
            int     bits;
            int     mask;
         public:
            int cmd;
            QString txt;
      };

   private:
      KeyEntry* addEntry(int ref, int key, int bits, int mask, int cmd, QString txt);
      void addKeyTrans();
      void readConfig();
      QPtrList<KeyEntry> tableX;
      QString m_hdr;
      QString m_path;
      QString m_id;
      int m_numb;
      bool m_fileRead;
      KeyTrans();
};

#endif
