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

#define BITS(x,v) ((((v)!=0)<<(x)))


class KeyTrans
{
public:
  KeyTrans();
  ~KeyTrans();
  static KeyTrans* defaultKeyTrans();
  static KeyTrans* fromFile(const char* path);
  static KeyTrans* find(int numb);
  static KeyTrans* find(const char* path);
public:
  static int count();
  static void loadAll();
public:
  bool findEntry(int key, int bits, int* cmd, const char** txt, int* len);
private:
  void addKeyTrans();
  static KeyTrans* fromDevice(QString path, QIODevice &buf);
public:
  class KeyEntry
  {
  public:
    KeyEntry(int ref, int key, int bits, int mask, int cmd, QString txt);
    ~KeyEntry();
  public:
    bool matches(int key, int bits, int mask);
    QString text();
  public:
    int ref;
  private:
    int     key;
    int     bits;
    int     mask;
  public:
    int cmd;
    QString txt;
  };
public:
  KeyEntry* addEntry(int ref, int key, int bits, int mask, int cmd, QString txt);
private:
  QList<KeyEntry> table;
public: //FIXME: we'd do better
  QString         hdr;
  int numb;
  QString path;
};

#endif
