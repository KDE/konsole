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
  double     tr_r, tr_g, tr_b;  // The ratio with which the pixels color component
			// is multiplied, i.e. if tr_r = tr_g = tr_b = 0.5, a
			// pixel as (255,0,32) (a red pixel) will be converted to 
			// (127,0,16)
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
