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
  bool       usetransparency;
  double     tr_x;
  int	     tr_r, tr_g, tr_b;
public:
  static ColorSchema* readSchema(const QString & path);
  static ColorSchema* find(const QString & path);
  static ColorSchema* find(int num);
  static ColorSchema* defaultSchema();
  static void         loadAllSchemas();
  static int          count();
public:
  void addSchema();
};

#endif
