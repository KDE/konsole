/*
  [kcmkonsole.C] Konsole Configurator for Kcontrol

  Copyright (c) 1998 by Lars Doelle.
  Artistic License applies.
*/


/*! /program

    These are some configuration pages for Kcontrol.
*/

#include "kcmkonsole.h"
#include "schema.h"
#include "qlayout.h"
#include "qpushbt.h"
#include "qpixmap.h"
#include "qslider.h"
#include <kiconloader.h>

#include <stdio.h>

#define HERE fprintf(stderr,"%s(%d): here\n",__FILE__,__LINE__);

//--| Overall apearance |-------------------------------------------------------

PageFrame::PageFrame(QWidget* parent) : QWidget(parent)
{
}

void PageFrame::Contents(const char* header, QWidget* body, const char* footer)
{
    QBoxLayout *topLayout = new QVBoxLayout( this, 5 ); 

    QLabel *title = new QLabel( header, this );
    title->setBuddy(title);
    title->setMinimumSize( title->sizeHint() );
    // Make  a big widget that will grab all space in the middle.
    topLayout->addWidget( title, 1 );
    topLayout->addWidget( body, 1000 );
    // Add a widget at the bottom.
    QLabel* sb = new QLabel(this);
    sb->setFrameStyle( QFrame::Box | QFrame::Sunken );
    topLayout->addWidget( sb , 2);
    QBoxLayout *sbl = new QHBoxLayout( sb,5,5 ); 
    QLabel* logo = new QLabel(sb);
    QPixmap pm = kapp->getIconLoader()->loadIcon(QString("konsole.xpm"));
    logo->setPixmap(pm);
    logo->setAlignment( AlignCenter );
    logo->setMinimumSize( logo->sizeHint() );
    sbl->addWidget( logo , 2);
    QLabel* footext = new QLabel(sb);
    footext->setText(footer);
    footext->setAlignment( WordBreak );
    sbl->addWidget( footext , 1000);
    sbl->activate();

    topLayout->activate();
}

PageFrame::~PageFrame()
{
}

//--| Schema configuration |----------------------------------------------------

GeneralPage::GeneralPage(QWidget* parent) : PageFrame(parent)
{
  QLabel *bigWidget = new QLabel( "This is work in progress.", this );
  bigWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  bigWidget->setAlignment( AlignCenter  );
  bigWidget->setBackgroundMode(PaletteBase);
//bigWidget->setMinimumHeight(400);
  Contents
  ( i18n( 
    "General Konsole settings"
    ),
    bigWidget,
    i18n(
    "{summary on konsole's general attributes.}"
    )
  );
}

GeneralPage::~GeneralPage()
{
}

//--| some algebra on colors |---------------------------------------------------

/*
   Please don't take this hack here too serious. It attempts
   to do a sort of tv set control for color adjustments.

   (The author hereby denies all rumours that this is an
    indication of secret ongoing works on a tv emulator.
    He also denies that konsole will be renamed to ktv. ;^)

   The color adjustment made here are based on an RGB cube.
   Black is at (0,0,0), while White is at (1,1,1).

   Arraging a subcube from (a,a,a) to (b,b,b), we treat the
   length of its diagonal as "contrast" and the location of
   it's center as "brightness".

   The diagonal of the subcube contains only different sorts of
   gray. By mapping the luminence of the colors to it's gray
   equivalent, we can make a sort of "color intensity" mapping
   also, that has the full colors at one and the gray levels
   at it's other end.
*/

class Tripel
{
public:
  Tripel();
  Tripel(float dia);
  Tripel(float r, float g, float b);
  Tripel(QColor c);
public: // all [0..1]
  float r;
  float g;
  float b;
public:
  QColor color();
public:
  Tripel scale(float f);
  static Tripel add(const Tripel &a, const Tripel &b);
  static Tripel linear(const Tripel &p0, const Tripel &p1, float f);
  Tripel togray(float f);
public:
  void print();
};

Tripel::Tripel()
{
  r = 0;
  g = 0;
  b = 0;
}

Tripel::Tripel(float dia)
{
  r = dia;
  g = dia;
  b = dia;
}

Tripel::Tripel(float r, float g, float b)
{
  this->r = r;
  this->g = g;
  this->b = b;
}

Tripel::Tripel(QColor c)
{
  this->r = c.red  () / 255.0;
  this->g = c.green() / 255.0;
  this->b = c.blue () / 255.0;
}

QColor Tripel::color()
{
  return QColor(r*255,g*255,b*255);
}

void Tripel::print()
{
  printf("Tripel(%4.2f,%4.2f,%4.2f)\n",r,g,b);
}

Tripel Tripel::scale(float f)
{
  return Tripel(f*r,f*g,f*b);
}

Tripel Tripel::add(const Tripel &a, const Tripel &b)
{
  return Tripel(a.r+b.r, a.g+b.g, a.b+b.b);
}

Tripel Tripel::linear(const Tripel &p0, const Tripel &p1, float f)
{
  return Tripel
  ( f*(p1.r - p0.r) + p0.r,
    f*(p1.b - p0.b) + p0.b,
    f*(p1.g - p0.g) + p0.g
  );
}

Tripel Tripel::togray(float f)
{
  // I've used the physiological luminescense factors here.
  // They appear not right with the phosphor of my monitor.
  // May be i should have set them all to 1/3, but i want
  // to get different sorts of gray for the colors also.
  // If your' tuning, note that they have to total to 1.
  Tripel rp = Tripel::linear(Tripel(0.37*r),Tripel(r,0,0),f);
  Tripel bp = Tripel::linear(Tripel(0.39*b),Tripel(0,b,0),f);
  Tripel gp = Tripel::linear(Tripel(0.24*g),Tripel(0,0,g),f);
  return Tripel::add( rp, Tripel::add( bp, gp ));
}

//--| Schema configuration |----------------------------------------------------


ColorTable::ColorTable(QWidget* parent, int lower, int upper) : QLabel(parent)
{
  //setFrameStyle( QFrame::Panel | QFrame::Sunken );
  //setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  this->lower = lower;
  this->upper = upper;
  setAlignment(AlignCenter);
  setBackgroundMode(PaletteBase);
  schema = (ColorSchema*)NULL;
  scale = 1;
}

//void ColorTable::resizeEvent(QResizeEvent* e)
//{
//}

void ColorTable::setSchema(ColorSchema* s)
{
  schema = s;
setText("");
  setBackgroundMode(schema?NoBackground:PaletteBase);
  if (!schema) return;
char* pa = strrchr(s->path.data(),'/');
setText(pa&&*pa?pa+1:"/* build-in schema */");
  update();
}

void ColorTable::paintEvent(QPaintEvent* e)
{
  // in the moment we don't care and paint the whole bunch
  // we don't care about all the tricks, also.
  QPainter paint;
  paint.begin( this );
  if (schema)
  for (int y = lower; y <= upper; y++)
  {
    QRect base = frameRect();
    int top = base.height()*((y-lower)+0)/(upper-lower+1);
    int bot = base.height()*((y-lower)+1)/(upper-lower+1);
    QRect rect(QPoint(base.left(),top),QPoint(base.right(),bot));
    QColor c0 = schema->table[y].color;
    float off   = shift * (1 - scale);
    Tripel t0(c0);
    Tripel t2(off);
    Tripel t3 = Tripel::add( t0.scale(scale), t2 );
    Tripel t4 = t3.togray(color);
    paint.fillRect(rect, t4.color() );
  }
//drawFrame(&paint);
  paint.end();
}

SchemaConfig::SchemaConfig(QWidget* parent) : PageFrame(parent)
{
  QLabel *bigWidget = new QLabel(this); //( "This is work in progress.", this );
  bigWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  bigWidget->setAlignment( AlignCenter  );

  QLabel *smlWidget = new QLabel( "This is work in progress.", bigWidget );
  smlWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  smlWidget->setAlignment( AlignCenter  );
  smlWidget->setBackgroundMode(PaletteBase);

  QGridLayout* topLayout = new QGridLayout( bigWidget, 2, 2, 5 ); 
  lbox = new QListBox(bigWidget); //FIXME: QT does not react on setFrameStyle
  //lbox->setFrameStyle( QFrame::Panel | QFrame::Sunken );
//colorTableW = new ColorTable(bigWidget,0,TABLE_COLORS-1);
  topLayout->setColStretch(0,4);
  topLayout->setColStretch(1,2);
  topLayout->setRowStretch(0,4);
  topLayout->setRowStretch(1,1);
//topLayout->addWidget( colorTableW, 0, 0 );
  topLayout->addWidget( lbox, 0, 1 );

  colorTableW[0] = new ColorTable(bigWidget, 0, 1);
  colorTableW[1] = new ColorTable(bigWidget, 2, 9);
  colorTableW[2] = new ColorTable(bigWidget,10,11);
  colorTableW[3] = new ColorTable(bigWidget,12,19);

  QGridLayout* clayout = new QGridLayout(2,2,5);
  topLayout->addLayout( clayout, 0,0 );
  clayout->setRowStretch(0,1);
  clayout->setRowStretch(1,4);
  clayout->addWidget(colorTableW[0], 0,0);
  clayout->addWidget(colorTableW[1], 1,0);
  clayout->addWidget(colorTableW[2], 0,1);
  clayout->addWidget(colorTableW[3], 1,1);

  QGridLayout* slayout = new QGridLayout(3,2,5);
  topLayout->addLayout( slayout, 1,0 );
  slayout->setColStretch(0,1);
  slayout->setColStretch(1,3);

  sl0 = new QSlider(0,100,10,0,QSlider::Horizontal,bigWidget);
  sl0->setTickmarks(QSlider::Below);
  slayout->addWidget(new QLabel(i18n("contrast"),bigWidget),0,0);
  slayout->addWidget(sl0,0,1);
  QObject::connect( sl0, SIGNAL(valueChanged(int)),
                    this, SLOT(sl0ValueChanged(int)) );

  sl1 = new QSlider(0,100,10,0,QSlider::Horizontal,bigWidget);
  sl1->setTickmarks(QSlider::Below);
  slayout->addWidget(new QLabel(i18n("brightness"),bigWidget),1,0);
  slayout->addWidget(sl1,1,1);
  QObject::connect( sl1, SIGNAL(valueChanged(int)),
                    this, SLOT(sl1ValueChanged(int)) );

  sl2 = new QSlider(0,100,10,0,QSlider::Horizontal,bigWidget);
  sl2->setTickmarks(QSlider::Below);
  slayout->addWidget(new QLabel(i18n("colourness"),bigWidget),2,0);
  slayout->addWidget(sl2,2,1);
  QObject::connect( sl2, SIGNAL(valueChanged(int)),
                    this, SLOT(sl2ValueChanged(int)) );

//topLayout->addMultiCellWidget( smlWidget, 1,1, 0,1 );
  topLayout->addWidget( smlWidget, 1,1 );
  ColorSchema::loadAllSchemas();
  for (int i = 0; i < ColorSchema::count(); i++)
  { ColorSchema* s = ColorSchema::find(i);
//  assert( s );
    lbox->insertItem(s->title.data());
  }
  topLayout->activate();
  QObject::connect( lbox, SIGNAL( highlighted(int) ),
                    this, SLOT( setSchema(int) ) );
                  

  Contents
  ( i18n(
    "Color Schema Management"
    ),
    bigWidget,
    i18n(
    "Color Schemas define a palette of colors together with further "
    "specifications of the rendering."
    )
  );
}

void SchemaConfig::sl0ValueChanged(int n)
{ int i;
  for (i = 0; i < 4; i++)
  {
    colorTableW[i]->scale = n / 100.0;
    colorTableW[i]->update();
  }
}

void SchemaConfig::sl1ValueChanged(int n)
{ int i;
  for (i = 0; i < 4; i++)
  {
    colorTableW[i]->shift = n / 100.0;
    colorTableW[i]->update();
  }
}

void SchemaConfig::sl2ValueChanged(int n)
{ int i;
  for (i = 0; i < 4; i++)
  {
    colorTableW[i]->color = n / 100.0;
    colorTableW[i]->update();
  }
}

void SchemaConfig::setSchema(int n)
{ int i;
  for (i = 0; i < 4; i++)
  {
    colorTableW[i]->setSchema(ColorSchema::find(n));
    colorTableW[i]->scale = 1.0;
    colorTableW[i]->shift = 0.5;
    colorTableW[i]->color = 1.0;
  }
  sl0->setValue(100);
  sl1->setValue(50);
  sl2->setValue(100);
}

SchemaConfig::~SchemaConfig()
{
}

//--| Session configuration |----------------------------------------------------

SessionConfig::SessionConfig(QWidget* parent) : PageFrame(parent)
{
  QLabel *bigWidget = new QLabel( "This is work in progress.", this );
  bigWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  bigWidget->setAlignment( AlignCenter  );
  bigWidget->setBackgroundMode(PaletteBase);
  Contents
  ( i18n(
    "Session Management"
    ),
    bigWidget,
    i18n(
    "Sessions are actually commands that can be executed from within "
    "konsole."
    )
  );
}

SessionConfig::~SessionConfig()
{
}


//--| Kcontrol pages //|--------------------------------------------------------

KcmKonsole::KcmKonsole(int &argc, char **argv, const char *name)
  : KControlApplication(argc, argv, name)
{
  if (runGUI())
  {
//  if (!pages || pages->contains("schemes"))
      addPage(schemes = new SchemaConfig(dialog),
              klocale->translate("&Color Schemes"),
              "kcmkonsole-not-written-yet.html");
//  if (!pages || pages->contains("general"))
      addPage(general = new GeneralPage(dialog),
              klocale->translate("&General"),
              "kcmkonsole-not-written-yet.html");
//  if (!pages || pages->contains("sessions"))
      addPage(sessions = new SessionConfig(dialog),
              klocale->translate("&Sessions"),
              "kcmkonsole-not-written-yet.html");

    if (schemes || sessions || general)
       dialog->show();
    else
    {
      fprintf(stderr, klocale->translate("usage:"));
      fprintf(stderr, "kcmkonsole [-init | schemes | general | sessions]\n");
      justInit = TRUE;
    }
  }
}


void KcmKonsole::init()
{
}


void KcmKonsole::apply()
{
}


void KcmKonsole::defaultValues()
{
}


int main(int argc, char **argv)
{
  KcmKonsole app(argc, argv, "kcmkonsole");
  app.setTitle(klocale->translate("Konsole Settings"));
  
  if (app.runGUI())
    return app.exec();
  else
    {
      app.init();
      return 0;
    }
}

#include "kcmkonsole.moc"
