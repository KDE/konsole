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
#include "qtableview.h"
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

//--| Schema configuration |----------------------------------------------------


ColorTable::ColorTable(QWidget* parent) : QFrame(parent)
{
  //setFrameStyle( QFrame::Panel | QFrame::Sunken );
  setFrameStyle( QFrame::WinPanel | QFrame::Sunken );
  setBackgroundMode(PaletteBase);
}

//void ColorTable::resizeEvent(QResizeEvent* e)
//{
//}

//void ColorTable::paintCell(QPainter* p, int row, int col)
//{
//}

SchemaConfig::SchemaConfig(QWidget* parent) : PageFrame(parent)
{
  QLabel *bigWidget = new QLabel(this); //( "This is work in progress.", this );
  bigWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  bigWidget->setAlignment( AlignCenter  );
  bigWidget->setBackgroundMode(PaletteBase);

  QLabel *smlWidget = new QLabel( "This is work in progress.", bigWidget );
  smlWidget->setFrameStyle( QFrame::Panel | QFrame::Sunken );
  smlWidget->setAlignment( AlignCenter  );
  smlWidget->setBackgroundMode(PaletteBase);

  QGridLayout* topLayout = new QGridLayout( bigWidget, 2, 2, 5 ); 
  lbox = new QListBox(bigWidget); //FIXME: QT does not react on setFrameStyle
  ColorTable* colorTableW = new ColorTable(bigWidget);
  topLayout->setColStretch(0,4);
  topLayout->setColStretch(1,2);
  topLayout->setRowStretch(0,4);
  topLayout->setRowStretch(1,1);
  topLayout->addWidget( colorTableW, 0, 0 );
  topLayout->addWidget( lbox, 0, 1 );
  topLayout->addMultiCellWidget( smlWidget, 1,1, 0,1 );
  ColorSchema::loadAllSchemas();
  for (int i = 0; i < ColorSchema::count(); i++)
  { ColorSchema* s = ColorSchema::find(i);
//  assert( s );
    lbox->insertItem(s->title.data());
  }
  topLayout->activate();

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
