/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [keytrans.h]             X Terminal Emulation                              */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#ifndef KEYTRANS_H
#define KEYTRANS_H

#include <qstring.h>
#include <qlist.h>
#include <qiodevice.h>

#define BITS_NewLine    0
#define BITS_BsHack     1
#define BITS_Ansi       2
#define BITS_AppCuKeys  3
#define BITS_Control    4
#define BITS_Shift      5
#define BITS_Alt        6
#define BITS_COUNT      7

#define CMD_send           0
#define CMD_emitSelection  1
#define CMD_scrollPageUp   2
#define CMD_scrollPageDown 3
#define CMD_scrollLineUp   4
#define CMD_scrollLineDown 5
#define CMD_prevSession    6
#define CMD_nextSession    7
#define CMD_newSession     8

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
      bool findEntry(int key, int bits, int* cmd, const char** txt, int* len);
      const QString& hdr()         {if (!m_fileRead) readConfig(); return m_hdr;}
      int numb()                   {return m_numb;}
      const QString& id() { return m_id;}

      class KeyEntry
      {
         public:
            KeyEntry(int ref, int key, int bits, int mask, int cmd, QString txt);
            ~KeyEntry();
            bool matches(int key, int bits, int mask);
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
      QList<KeyEntry> tableX;
      QString m_hdr;
      QString m_path;
      QString m_id;
      int m_numb;
      bool m_fileRead;
      KeyTrans();
};

#endif
