// [schema.C]

/*! \class ColorSchema

    This is new stuff, so no docs yet.

    The identifier is the path. `numb' is guarantied to range from 0 to
    count-1. Note when reloading the path can be assumed to still identify
    a know schema, while the `numb' may vary.
*/

#include "schema.h"
#include <stdio.h>
#include "kapp.h"
#include <kdebug.h>
#include <qfile.h>
#include <qdir.h>
#include <stdlib.h>
#include <kstddirs.h>
#include <kglobal.h>
#include <klocale.h>
#include <kstaticdeleter.h>

#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)

static int schema_serial = 0; //FIXME: remove,localize

static QIntDict<ColorSchema> * numb2schema = 0L;
static QDict<ColorSchema>   * path2schema = 0L;

static KStaticDeleter< QIntDict<ColorSchema> > n2sd;
static KStaticDeleter< QDict<ColorSchema> > p2sd;

template class QIntDict<ColorSchema>;
template class QDict<ColorSchema>;

ColorSchema* ColorSchema::readSchema(const QString & path)
{ FILE* sysin = fopen(QFile::encodeName(path),"r");
  char line[100]; int i;

  if (!sysin) return NULL;
  //
  ColorSchema* res = new ColorSchema;
  res->path = path;
  res->numb = schema_serial++;
  for (i = 0; i < TABLE_COLORS; i++)
  {
    res->table[i].color       = QColor(0,0,0);
    res->table[i].transparent = 0;
    res->table[i].bold        = 0;
  }
  res->title     = "[missing title]";
  res->imagepath = "";
  res->alignment = 1;
  res->usetransparency = false;
  res->tr_x = 0.0;
  res->tr_r = 0;
  res->tr_g = 0;
  res->tr_b = 0;

  while (fscanf(sysin,"%80[^\n]\n",line) > 0)
  {
    if (strlen(line) > 5)
    {
      if (!strncmp(line,"title",5))
      {
        res->title = i18n(line+6);
      }
      if (!strncmp(line,"image",5))
      { char rend[100], path[100]; int attr = 1;
        if (sscanf(line,"image %s %s",rend,path) != 2)
          continue;
        if (!strcmp(rend,"tile"  )) attr = 2; else
        if (!strcmp(rend,"center")) attr = 3; else
        if (!strcmp(rend,"full"  )) attr = 4; else
          continue;

        res->imagepath = locate("wallpaper", path);
        res->alignment = attr;
      }
      if (!strncmp(line,"transparency",12))
      { float rx;
        int rr, rg, rb;

	// Transparency needs 4 parameters: fade strength and the 3
	// components of the fade color.
        if (sscanf(line,"transparency %g %d %d %d",&rx,&rr,&rg,&rb) != 4)
          continue;
	res->usetransparency=true;
	res->tr_x=rx;
	res->tr_r=rr;
	res->tr_g=rg;
	res->tr_b=rb;
      }
      if (!strncmp(line,"color",5))
      { int fi,cr,cg,cb,tr,bo;
        if(sscanf(line,"color %d %d %d %d %d %d",&fi,&cr,&cg,&cb,&tr,&bo) != 6)
          continue;
        if (!(0 <= fi && fi <= TABLE_COLORS)) continue;
        if (!(0 <= cr && cr <= 255         )) continue;
        if (!(0 <= cg && cg <= 255         )) continue;
        if (!(0 <= cb && cb <= 255         )) continue;
        if (!(0 <= tr && tr <= 1           )) continue;
        if (!(0 <= bo && bo <= 1           )) continue;
        res->table[fi].color       = QColor(cr,cg,cb);
        res->table[fi].transparent = tr;
        res->table[fi].bold        = bo;
      }
      if (!strncmp(line,"sysfg",5))
      { int fi,tr,bo;
        if(sscanf(line,"sysfg %d %d %d",&fi,&tr,&bo) != 3)
          continue;
        if (!(0 <= fi && fi <= TABLE_COLORS)) continue;
        if (!(0 <= tr && tr <= 1           )) continue;
        if (!(0 <= bo && bo <= 1           )) continue;
        res->table[fi].color       = kapp->palette().normal().text();
        res->table[fi].transparent = tr;
        res->table[fi].bold        = bo;
      }
      if (!strncmp(line,"sysbg",5))
      { int fi,tr,bo;
        if(sscanf(line,"sysbg %d %d %d",&fi,&tr,&bo) != 3)
          continue;
        if (!(0 <= fi && fi <= TABLE_COLORS)) continue;
        if (!(0 <= tr && tr <= 1           )) continue;
        if (!(0 <= bo && bo <= 1           )) continue;
        res->table[fi].color       = kapp->palette().normal().base();
        res->table[fi].transparent = tr;
        res->table[fi].bold        = bo;
      }
    }
  }
  fclose(sysin);
  return res;
}

ColorSchema* ColorSchema::find(int numb)
{
  ColorSchema* res = numb2schema->find(numb);
  return res ? res : numb2schema->find(0);
}

ColorSchema* ColorSchema::find(const QString & path)
{
  QString real_loc(path);
  if ( !path.isEmpty() && path[0] != '/' ) {
    real_loc = locate("appdata", path);
    if ( real_loc.isEmpty() )
      real_loc = path;
  }
  ColorSchema* res = path2schema->find(real_loc.latin1());
  return res ? res : numb2schema->find(0);
}

int ColorSchema::count()
{
  return numb2schema->count();
}

void ColorSchema::addSchema()
{
  numb2schema->insert(numb,this);
  path2schema->insert(path,this);
}

static const ColorEntry default_table[TABLE_COLORS] =
 // The following are almost IBM standard color codes, with some slight
 // gamma correction for the dim colors to compensate for bright X screens.
 // It contains the 8 ansiterm/xterm colors in 2 intensities.
{
    ColorEntry( QColor(0x00,0x00,0x00), 0, 0 ), ColorEntry( QColor(0xFF,0xFF,0xFF), 1, 0 ), // Dfore, Dback
    ColorEntry( QColor(0x00,0x00,0x00), 0, 0 ), ColorEntry( QColor(0xB2,0x18,0x18), 0, 0 ), // Black, Red
    ColorEntry( QColor(0x18,0xB2,0x18), 0, 0 ), ColorEntry( QColor(0xB2,0x68,0x18), 0, 0 ), // Green, Yellow
    ColorEntry( QColor(0x18,0x18,0xB2), 0, 0 ), ColorEntry( QColor(0xB2,0x18,0xB2), 0, 0 ), // Blue,  Magenta
    ColorEntry( QColor(0x18,0xB2,0xB2), 0, 0 ), ColorEntry( QColor(0xB2,0xB2,0xB2), 0, 0 ), // Cyan,  White
    // intensive
    ColorEntry( QColor(0x00,0x00,0x00), 0, 1 ), ColorEntry( QColor(0xFF,0xFF,0xFF), 1, 0 ),
    ColorEntry( QColor(0x68,0x68,0x68), 0, 0 ), ColorEntry( QColor(0xFF,0x54,0x54), 0, 0 ),
    ColorEntry( QColor(0x54,0xFF,0x54), 0, 0 ), ColorEntry( QColor(0xFF,0xFF,0x54), 0, 0 ),
    ColorEntry( QColor(0x54,0x54,0xFF), 0, 0 ), ColorEntry( QColor(0xFF,0x54,0xFF), 0, 0 ),
    ColorEntry( QColor(0x54,0xFF,0xFF), 0, 0 ), ColorEntry( QColor(0xFF,0xFF,0xFF), 0, 0 )
};

ColorSchema* ColorSchema::defaultSchema()
{
  ColorSchema* res = new ColorSchema;
  res->path = "";
  res->numb = 0;
  res->title = "Konsole Default";
  res->imagepath = ""; // background pixmap
  res->alignment = 1;  // none
  res->usetransparency = false; // not use pseudo-transparency by default
  res->tr_r = res->tr_g = res->tr_b = 0; // just to be on the safe side
  res->tr_x = 0.0;
  for (int i = 0; i < TABLE_COLORS; i++)
    res->table[i] = default_table[i];
  return res;
}

void ColorSchema::loadAllSchemas()
{

  if ( !numb2schema )
    numb2schema = n2sd.setObject(new QIntDict<ColorSchema>);
  if ( !path2schema )
    path2schema = p2sd.setObject(new QDict<ColorSchema>);

  defaultSchema()->addSchema();
  schema_serial = 1;
  QStringList lst = KGlobal::dirs()->findAllResources("appdata", "*.schema");

  for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it ) {
    ColorSchema* sc = ColorSchema::readSchema(*it);
    if (sc) sc->addSchema();
  }
}
