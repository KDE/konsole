/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [TECommon.h]                  Common Definitions                           */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

/*! \file TECommon.h
    \brief Definitions shared between TEScreen and TEWidget.
*/

#ifndef TECOMMON_H
#define TECOMMON_H

#include <qcolor.h>

#ifndef BOOL
typedef int BOOL;
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef UINT8
typedef unsigned char UINT8;
#endif

#ifndef UINT16
typedef unsigned short UINT16;
#endif

// Attributed Character Representations ///////////////////////////////

// Colors

#define BASE_COLORS   (2+8)
#define INTENSITIES   2
#define TABLE_COLORS  (INTENSITIES*BASE_COLORS)

#define DEFAULT_FORE_COLOR 0
#define DEFAULT_BACK_COLOR 1

#define DEFAULT_RENDITION  0
#define RE_BOLD            (1 << 0)
#define RE_BLINK           (1 << 1)
#define RE_UNDERLINE       (1 << 2)
#define RE_REVERSE         (1 << 3) // Screen only
#define RE_INTENSIVE       (1 << 3) // Widget only

/*! \class ca
 *  \brief a character with rendition attributes.
*/

class ca
{
public:
  inline ca(UINT16 _c = ' ',
            UINT8 _f = DEFAULT_FORE_COLOR,
            UINT8 _b = DEFAULT_BACK_COLOR,
            UINT8 _r = DEFAULT_RENDITION)
       : c(_c), f(_f), b(_b), r(_r) {}
public:
  UINT16 c; // character
  UINT8  f; // foreground color
  UINT8  b; // background color
  UINT8  r; // rendition
public:
  friend BOOL operator == (ca a, ca b);
  friend BOOL operator != (ca a, ca b);
};

inline BOOL operator == (ca a, ca b)
{
  return a.c == b.c && a.f == b.f && a.b == b.b && a.r == b.r;
}

inline BOOL operator != (ca a, ca b)
{
  return a.c != b.c || a.f != b.f || a.b != b.b || a.r != b.r;
}

/*!
*/
struct ColorEntry
{
  ColorEntry(QColor c, bool tr, bool b) : color(c), transparent(tr), bold(b) {}
  ColorEntry() : transparent(false), bold(false) {} // default constructors
  void operator=(const ColorEntry& rhs) { 
       color = rhs.color; 
       transparent = rhs.transparent; 
       bold = rhs.bold; 
  }
  QColor color;
  bool   transparent; // if used on bg
  bool   bold;        // if used on fg
};

#endif // TECOMMON_H
