/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [TEHistory.H]                   History Buffer                             */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

#ifndef TEHISTORY_H
#define TEHISTORY_H

#include "TECommon.h"

/*
   An extendable tmpfile(1) based buffer.
*/
class HistoryBuffer
{
public:
  HistoryBuffer();
 ~HistoryBuffer();

public:
  void setScroll(bool on);
  bool hasScroll();

public:
  void add(const unsigned char* bytes, int len);
  void get(unsigned char* bytes, int len, int loc);
  int  len();

private:
  int  ion;
  int  length;
};

class HistoryScroll
{
public:
  HistoryScroll();
 ~HistoryScroll();

public:
  void setScroll(bool on);
  bool hasScroll();

public: // access to history
  int  getLines();
  int  getLineLen(int lineno);
  ca   getCell(int lineno, int colno);

public: // adding lines.
  void addCell(ca a);
  void addLine();

private:
  int startOfLine(int lineno);
  HistoryBuffer index; // lines Row(int)
  HistoryBuffer cells; // text  Row(ca)
};

#endif // TEHISTORY_H
