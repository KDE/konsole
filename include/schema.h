#ifndef SCHEMA_include
#define SCHEMA_include

#include <qstring.h>
#include "TECommon.h"
#include <qdict.h>
#include <qintdict.h>

struct ColorSchema
{
public:
  QString    path;
  int        numb;
  QString    title;
  QString    imagepath;
  int        alignment;
  ColorEntry table[TABLE_COLORS];
public:
  static ColorSchema* readSchema(const char* path);
  static ColorSchema* find(const char* path);
  static ColorSchema* find(int num);
  static ColorSchema* defaultSchema();
  static void         loadAllSchemas();
  static int          count();
public:
  void addSchema();
};

#endif
