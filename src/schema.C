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
#include <qdir.h>

static int schema_serial = 0; //FIXME: remove,localize

static QIntDict<ColorSchema> numb2schema;
static QDict<ColorSchema>    path2schema;

ColorSchema* ColorSchema::readSchema(const char* path)
{ FILE* sysin = fopen(path,"r");
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
  //
  while (fscanf(sysin,"%80[^\n]\n",line) > 0)
  {
    if (strlen(line) > 5)
    {
      if (!strncmp(line,"title",5))
      {
        res->title = line+6;
      }
      if (!strncmp(line,"image",5))
      { char rend[100], path[100]; int attr = 1;
        if (sscanf(line,"image %s %s",rend,path) != 2)
          continue;
        if (!strcmp(rend,"tile"  )) attr = 2; else
        if (!strcmp(rend,"center")) attr = 3; else
        if (!strcmp(rend,"full"  )) attr = 4; else
          continue;

        // if this is not an absolute filename, prepend the wallpaper dir
        if (path[0] != '/') res->imagepath = kapp->kde_wallpaperdir() + '/';
        res->imagepath += path;
        res->alignment = attr;
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
        res->table[fi].color       = kapp->textColor;
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
        res->table[fi].color       = kapp->backgroundColor;
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
  ColorSchema* res = numb2schema.find(numb);
  return res ? res : numb2schema.find(0);
}

ColorSchema* ColorSchema::find(const char* path)
{
  ColorSchema* res = path2schema.find(path);
  return res ? res : numb2schema.find(0);
}

int ColorSchema::count()
{
  return numb2schema.count();
}

void ColorSchema::addSchema()
{
  numb2schema.insert(numb,this);
  path2schema.insert(path.data(),this);
}

static const ColorEntry default_table[TABLE_COLORS] =
 // The following are almost IBM standard color codes, with some slight
 // gamma correction for the dim colors to compensate for bright X screens.
 // It contains the 8 ansiterm/xterm colors in 2 intensities.
{
    { QColor(0x00,0x00,0x00), 0, 0 }, { QColor(0xFF,0xFF,0xFF), 1, 0 }, // Dfore, Dback
    { QColor(0x00,0x00,0x00), 0, 0 }, { QColor(0xB2,0x18,0x18), 0, 0 }, // Black, Red
    { QColor(0x18,0xB2,0x18), 0, 0 }, { QColor(0xB2,0x68,0x18), 0, 0 }, // Green, Yellow
    { QColor(0x18,0x18,0xB2), 0, 0 }, { QColor(0xB2,0x18,0xB2), 0, 0 }, // Blue,  Magenta
    { QColor(0x18,0xB2,0xB2), 0, 0 }, { QColor(0xB2,0xB2,0xB2), 0, 0 }, // Cyan,  White
    // intensive
    { QColor(0x00,0x00,0x00), 0, 1 }, { QColor(0xFF,0xFF,0xFF), 1, 0 },
    { QColor(0x68,0x68,0x68), 0, 0 }, { QColor(0xFF,0x54,0x54), 0, 0 }, 
    { QColor(0x54,0xFF,0x54), 0, 0 }, { QColor(0xFF,0xFF,0x54), 0, 0 }, 
    { QColor(0x54,0x54,0xFF), 0, 0 }, { QColor(0xFF,0x54,0xFF), 0, 0 },
    { QColor(0x54,0xFF,0xFF), 0, 0 }, { QColor(0xFF,0xFF,0xFF), 0, 0 }
};

ColorSchema* ColorSchema::defaultSchema()
{
  ColorSchema* res = new ColorSchema;
  res->path = "";
  res->numb = 0;
  res->title = "Konsole Default";
  res->imagepath = ""; // background pixmap
  res->alignment = 1;  // none
  for (int i = 0; i < TABLE_COLORS; i++)
    res->table[i] = default_table[i];
  return res;
}

void ColorSchema::loadAllSchemas()
{
  defaultSchema()->addSchema();
  schema_serial = 1;
  for (int local=0; local<=1; local++)
  {
    // KApplication could support this technique better
    QString path = local
                 ? kapp->localkdedir() + "/share/apps/konsole"
                 : kapp->kde_datadir() + "/konsole";
    QDir d( path );
    if (d.exists())
    {
      d.setFilter( QDir::Files | QDir::Readable );
      d.setNameFilter( "*.schema" );
      const QFileInfoList *list = d.entryInfoList();
      QFileInfoListIterator it( *list );      // create list iterator
      for(QFileInfo *fi; (fi=it.current()); ++it )
      {
        ColorSchema* sc = ColorSchema::readSchema(fi->filePath());
        if (sc) sc->addSchema();
      }
    }
  }
}
