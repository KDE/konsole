/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [rootBg.h]                 RootPixmap class to manage KBgndwm pixmaps      */
/*                              and transparent widgets                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1999      by Antonio Larrosa <larrosa@kde.org>               */
/*                                                                            */
/* This file is part of Konsole, an X terminal.                               */
/*                                                                            */
/* The material contained in contains portions (with some modifications) from */
/* kbgndwm, which is copyright (C) 1997 Martin Jones and (C) 1998 Matej Koss  */
/*                                                                            */
/* -------------------------------------------------------------------------- */
 
/*! \class RootPixmap 
 
    \brief RootPixmap class
 
    The class RootPixmap handles the Pixmap which kbgndwm paints at the root window. 
    It is responsible for the transparency, pixmap shading and these kind of things.
*/  
 
#include "qdir.h"
#include <kapp.h>
#include <kconfig.h>
#include <kwm.h>
#include <qfileinfo.h>
#include <qpainter.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>
#include <qimage.h>
#include <stdlib.h>
#include <qnamespace.h>
#include <klocale.h>
#include "config-kbgndwm.h"
#include "rootBg.h"
#include <kglobal.h>
#include <kstddirs.h>
#include <kpixmapeffect.h>


// By default, when you assign a background to your widget, the pixmap has
// the width and height of your widget, this cause some "curious" effects when
// resizing (tiled pixmap), so you can define ROOTPIXMAP_FAST_RESIZE, to 
// assign always a pixmap which has width = bgWidth-x and height = bgHeight-y, 
// that is, a pixmap which is larger than your widget, and so, you cannot see
// tiling
//#define ROOTPIXMAP_FAST_RESIZE

RootPixmap::RootPixmap()
{
bgPixmap=0L;
}

RootPixmap::~RootPixmap()
{
if (bgPixmap) delete bgPixmap;
}

void RootPixmap::readSettings(int num)
{
/*

  This function has been copied from kbgndwm (bg.cpp) and highly modified, 
  mainly, not to save anything in the config file as kbgndwm did   -- Antonio

*/

  bool hasPm = false;

/*
  TODO: This should be useful for optimization in case of oneDesktopMode is true

  KConfig config2(KApplication::kde_configdir() + "/kcmdisplayrc",
		  KApplication::localconfigdir() + "/kcmdisplayrc");

  config2.setGroup( "Desktop Common" );
  bool oneDesktopMode = config2.readBoolEntry( "OneDesktopMode", false );
  int oneDesk = config2.readNumEntry( "DeskNum", 0 );
*/
  dsk = num;

  QString tmpf;
  tmpf.sprintf("desktop%drc", num);

  KConfig config(tmpf);

  bool randomMode = false;
  int randomDesk=0;

  config.setGroup( "Common" );
  randomMode = config.readBoolEntry( "RandomMode", false );
  
  if ( randomMode ) {
    int count = config.readNumEntry( "Count", 1 );
    bool inorder = config.readBoolEntry( "InOrder", true);
    bool useDir = config.readBoolEntry( "UseDir", true );
    
    if ( useDir ) {
      
      QStringList list = KGlobal::dirs()->findAllResources("wallpaper");
      
      count = list.count();
      if ( inorder ) {
	randomDesk = config.readNumEntry( "Item", 0 );
	randomDesk++;
	if ( randomDesk >= count ) randomDesk = 0;
      } else if ( count > 0 )
	randomDesk = random() % count;
      
      color1 = QColor(DEFAULT_COLOR_1);
      gfMode = DEFAULT_COLOR_MODE;
      orMode = DEFAULT_ORIENTATION_MODE;
      wpMode = DEFAULT_WALLPAPER_MODE;
      bUseWallpaper = true;

      hasPm = true;
      
      return;
    }
    else if ( inorder ) {
      randomDesk = config.readNumEntry( "Item", DEFAULT_DESKTOP );
      randomDesk++;
      if ( randomDesk >= count ) randomDesk = DEFAULT_DESKTOP;
    }
    else if ( count > 0 )
      randomDesk = random() % count;
    
  }
  else
    randomDesk = DEFAULT_DESKTOP;
  
  tmpf.sprintf("Desktop%d", randomDesk);
  config.setGroup( tmpf );

  QString str;

  str = config.readEntry( "Color1", DEFAULT_COLOR_1 );
  color1.setNamedColor( str );

  str = config.readEntry( "Color2", DEFAULT_COLOR_2 );
  color2.setNamedColor( str );

  gfMode = DEFAULT_COLOR_MODE;
  str = config.readEntry( "ColorMode", "unset" );
  if ( str == "Gradient" ) {
      gfMode = Gradient;
      hasPm = true;
  }
  else if (str == "Pattern") {
    gfMode = Pattern;
    QStrList strl;
    config.readListEntry("Pattern", strl);
    uint size = strl.count();
    if (size > 8) size = 8;
    uint i = 0;
    for (i = 0; i < 8; i++)
      pattern[i] = (i < size) ? QString(strl.at(i)).toUInt() : 255;
  }

  orMode = DEFAULT_ORIENTATION_MODE;
  str = config.readEntry( "OrientationMode", "unset" );
  if ( str == "Landscape" )
    orMode = Landscape;

  wpMode = DEFAULT_WALLPAPER_MODE;
  str = config.readEntry( "WallpaperMode", "unset" );
  if ( str == "Mirrored" )
    wpMode = Mirrored;
  else if ( str == "CenterTiled" )
    wpMode = CenterTiled;
  else if ( str == "Centred" )
    wpMode = Centred;
  else if ( str == "CentredBrick" )
    wpMode = CentredBrick;
  else if ( str == "CentredWarp" )
    wpMode = CentredWarp;
  else if ( str == "CentredMaxpect" )
    wpMode = CentredMaxpect;
  else if ( str == "SymmetricalTiled" )
    wpMode = SymmetricalTiled;
  else if ( str == "SymmetricalMirrored" )
    wpMode = SymmetricalMirrored;
  else if ( str == "Scaled" )
    wpMode = Scaled;

  wallpaper = DEFAULT_WALLPAPER;
  bUseWallpaper = config.readBoolEntry( "UseWallpaper", DEFAULT_WALLPAPER_MODE );
  if ( bUseWallpaper )
    wallpaper = config.readEntry( "Wallpaper", DEFAULT_WALLPAPER );

  hasPm = true;
}

QPixmap *RootPixmap::loadWallpaper()
{

  if( !bUseWallpaper ) return 0;

  QString filename = locate("wallpaper", wallpaper);

  KPixmap *wpPixmap = new KPixmap;

  if ( wpPixmap->load( filename, QString::null, KPixmap::LowColor ) == FALSE )
    {
      delete wpPixmap;
      wpPixmap = 0;
    }

  return wpPixmap;
}



void RootPixmap::generateBackground(bool shade, double r, double g, double b)
{
/*

  This function has been copied from kbgndwm (bg.cpp) and highly modified, 
  mainly, to make bgPixmap always be full width and full height  -- Antonio

*/
  QPixmap *wpPixmap = loadWallpaper();

  uint w=0, h=0;
  w = QApplication::desktop()->width();
  h = QApplication::desktop()->height();

  bgPixmap = new QPixmap(w,h);

  printf("bb %dx%d\n",w,h);

  if ( !wpPixmap || (wpMode == Centred) || (wpMode == CentredBrick) ||
       (wpMode == CentredWarp) || ( wpMode == CentredMaxpect) ) {

    switch (gfMode) {

    case Gradient:
      {
	int numColors = 4;
	if ( QColor::numBitPlanes() > 8 )
	  numColors = 16;
		
	KPixmap pmDesktop;
		
	if ( orMode == Portrait ) {

	  pmDesktop.resize( 20, QApplication::desktop()->height() );
	  KPixmapEffect::gradient(pmDesktop, color1, color2, 
				  KPixmapEffect::VerticalGradient, numColors );

	} else {

	  pmDesktop.resize( QApplication::desktop()->width(), 20 );
	  KPixmapEffect::gradient(pmDesktop, color1, color2, 
				  KPixmapEffect::HorizontalGradient, 
				  numColors);
		
	}
        if (shade) shadePixmap(&pmDesktop,r,g,b); 

	delete bgPixmap;
	bgPixmap = new QPixmap(w,h);
		
	if ( orMode == Portrait ) {
	    for (uint pw = 0; pw <= w/20; pw += pmDesktop.width())
	      bitBlt( bgPixmap, pw*20, 0, &pmDesktop, 0, 0,
		      pmDesktop.width(), h);
	} else {
	    for (uint ph = 0; ph <= h/20; ph += pmDesktop.height()) {
	      debug("land %d",ph);
	      bitBlt( bgPixmap, 0, ph*20, &pmDesktop, 0, 0,
		      w, pmDesktop.height());
	  }
	}
      }
      break;
	
    case Flat:
      {
	delete bgPixmap;
	bgPixmap = new QPixmap(w, h);
        if (shade) shadeColor(&color1,r,g,b); 
        bgPixmap->fill( color1 );
      }
      break;
	
    case Pattern:
      {
	QPixmap tile(8, 8);
	tile.fill(color2);
	QPainter pt;
	pt.begin(&tile);
	pt.setBackgroundColor( color2 );
	pt.setPen( color1 );
		
	for (int y = 0; y < 8; y++) {
	  uint v = pattern[y];
	  for (int x = 0; x < 8; x++) {
	    if ( v & 1 )
	      pt.drawPoint(7 - x, y);
	    v /= 2;
	  }
	}
	pt.end();
        if (shade) shadePixmap(&tile,r,g,b); 

	delete bgPixmap;
	bgPixmap = new QPixmap(w,h);
	uint sx, sy = 0;
	while (sy < h) {
	  sx = 0;
	  while (sx < w) {
	    bitBlt( bgPixmap, sx, sy, &tile, 0, 0, 8, 8);
	    sx += 8;
	  }
	  sy += 8;
	}
	break;
      }
    }
	
  }

  if ( wpPixmap )
    { 
      if (shade) shadePixmap(wpPixmap,r,g,b);
	
      if ( ( wpPixmap->width() > (int)w || wpPixmap->height() > (int)h ||
	   wpMode == CentredMaxpect ) &&
	   wpMode != Scaled ) {
	// shrink if image is bigger than desktop or CentredMaxpect
	float sc;
	float S = (float)h / (float)w ;
	float I = (float)wpPixmap->height() / (float)wpPixmap->width() ;
	
	if (S < I)
	  sc= (float)h / (float)wpPixmap->height();
	else
	  sc= (float)w / (float)wpPixmap->width();
	
	QWMatrix scaleMat;
	scaleMat.scale(sc,sc);

	QPixmap tmp2 = wpPixmap->xForm( scaleMat );
	wpPixmap->resize( tmp2.width(), tmp2.height() );
	bitBlt( wpPixmap, 0, 0, &tmp2 );
      }

      switch ( wpMode )
	{

	case Tiled:
	  {
//	    bgPixmap->resize( wpPixmap->width(), wpPixmap->height() );
//	    bgPixmap->fill( color1 );
	 bgPixmap->resize(w, h);
	 int x,y=0;     
                  while (y<bgPixmap->height()+wpPixmap->height())
                    {
                        x=0;
                        while (x<bgPixmap->width()+wpPixmap->width())
                        {
                            bitBlt(bgPixmap,x,y,wpPixmap,0,0,wpPixmap->width(),wpPixmap->height(),Qt::CopyROP);
                            x+=wpPixmap->width();
                        }
                        y+=wpPixmap->height();
                    }
//	    bitBlt( bgPixmap, 0, 0, wpPixmap );
	  }
	  break;

	case Mirrored:
	  {
	    int w = wpPixmap->width();
	    int h = wpPixmap->height();

	    bgPixmap->resize( w * 2, h * 2);

	    /* quadrant 2 */
 	    bitBlt( bgPixmap, 0, 0, wpPixmap );
	
	    /* quadrant 1 */
	    QWMatrix S(-1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F);
	    QPixmap newp = wpPixmap->xForm( S );
	    bitBlt( wpPixmap, 0, 0, &newp );
	    bitBlt( bgPixmap, w, 0, wpPixmap );

	    /* quadrant 4 */
	    S.setMatrix(1.0F, 0.0F, 0.0F, -1.0F, 0.0F, 0.0F);
	    newp = wpPixmap->xForm( S );
	    bitBlt( wpPixmap, 0, 0, &newp );
	    bitBlt( bgPixmap, w, h, wpPixmap );

	    /* quadrant 3 */
	    S.setMatrix(-1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F);
	    newp = wpPixmap->xForm( S );
	    bitBlt( wpPixmap, 0, 0, &newp );
	    bitBlt( bgPixmap, 0, h, wpPixmap );

	  }
	  break;

	case SymmetricalTiled:
	case SymmetricalMirrored:
	  {
	    int fliph = 0;
	    int flipv = 0;
	    uint w0 = wpPixmap->width();
	    uint h0 = wpPixmap->height();

	    bgPixmap->resize(w, h);

	    if (w == w0) {
	      /* horizontal center line */
	      int y, ay;

	      y = h0 - ((h/2)%h0); /* Starting point in picture to copy */
	      ay = 0;    /* Vertical anchor point */
	      while (ay < (int)h) {
		bitBlt( bgPixmap, 0, ay, wpPixmap, 0, y );
		ay += h0 - y;
		y = 0;
		if ( wpMode == SymmetricalMirrored ) {
		  QWMatrix S(1.0F, 0.0F, 0.0F, -1.0F, 0.0F, 0.0F);
		  QPixmap newp = wpPixmap->xForm( S );
		  bitBlt( wpPixmap, 0, 0, &newp );
		  flipv = !flipv;
		}
	      }
	    }
	    else if (h == h0) {
	      /* vertical centerline */
	      int x, ax;

	      x = w0 - ((w/2)%w0); /* Starting point in picture to copy */
	      ax = 0;    /* Horizontal anchor point */
	      while (ax < (int)w) {
		bitBlt( bgPixmap, ax, 0, wpPixmap, x, 0 );
		ax += w0 - x;
		x = 0;
		if ( wpMode == SymmetricalMirrored ) {
		  QWMatrix S(-1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F);
		  QPixmap newp = wpPixmap->xForm( S );
		  bitBlt( wpPixmap, 0, 0, &newp );
		  fliph = !fliph;
		}
	      }
	    }
	    else {
	      /* vertical and horizontal centerlines */
	      int x,y, ax,ay;

	      y = h0 - ((h/2)%h0); /* Starting point in picture to copy */
	      ay = 0;    /* Vertical anchor point */

	      while (ay < (int)h) {
		x = w0 - ((w/2)%w0);/* Starting point in picture to cpy */
		ax = 0;    /* Horizontal anchor point */
		while (ax < (int)w) {
		  bitBlt( bgPixmap, ax, ay, wpPixmap, x, y );
		  if ( wpMode == SymmetricalMirrored ) {
		    QWMatrix S(-1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F);
		    QPixmap newp = wpPixmap->xForm( S );
		    bitBlt( wpPixmap, 0, 0, &newp );
		    fliph = !fliph;
		  }
		  ax += w0 - x;
		  x = 0;
		}
		if ( wpMode == SymmetricalMirrored ) {
		  QWMatrix S(1.0F, 0.0F, 0.0F, -1.0F, 0.0F, 0.0F);
		  QPixmap newp = wpPixmap->xForm( S );
		  bitBlt( wpPixmap, 0, 0, &newp );
		  flipv = !flipv;
		  if (fliph) {   /* leftmost image is always non-hflipped */
		    S.setMatrix(-1.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F);
		    newp = wpPixmap->xForm( S );
		    bitBlt( wpPixmap, 0, 0, &newp );
		    fliph = !fliph;
		  }
		}
		ay += h0 - y;
		y = 0;
	      }
	    }
	  }
	  break;
	
	case CenterTiled:
	  {
	    int i, j, x, y, w0, h0, ax, ay, w1, h1, offx, offy;

	    bgPixmap->resize(w, h);

	    w0 = wpPixmap->width();  h0 = wpPixmap->height();

	    /* compute anchor pt (top-left coords of top-left-most pic) */
	    ax = (w-w0)/2;  ay = (h-h0)/2;
	    while (ax>0) ax = ax - w0;
	    while (ay>0) ay = ay - h0;

	    for (i=ay; i < (int)h; i+=h0) {
	      for (j=ax; j < (int)w; j+=w0) {
		/* if image goes off tmpPix, only draw subimage */
	
		x = j;  y = i;  w1 = w0;  h1 = h0;  offx = offy = 0;
		if (x<0)           { offx = -x;  w1 -= offx;  x = 0; }
		if (x+w1>w0) { w1 = (w0-x); }

		if (y<0)           { offy = -y;  h1 -= offy;  y = 0; }
		if (y+h1>h0)    { h1 = (h0-y); }
	
		bitBlt( bgPixmap, x, y, wpPixmap, offx, offy );
	      }
	    }

	  }
	  break;
		
	case Centred:
	case CentredMaxpect:
	  {
	    bitBlt( bgPixmap, ( w - wpPixmap->width() ) / 2,
		    ( h - wpPixmap->height() ) / 2, wpPixmap, 0, 0,
		    wpPixmap->width(), wpPixmap->height() );
	  }
	  break;
		
	case Scaled:
	  {
	    float sx = (float)w / wpPixmap->width();
	    float sy = (float)h / wpPixmap->height();
			
	    bgPixmap->resize( w, h );
	    bgPixmap->fill( color1 );
			
	    QWMatrix matrix;
	    matrix.scale( sx, sy );
	    QPixmap newp = wpPixmap->xForm( matrix );
	    bitBlt( bgPixmap, 0, 0, &newp );
	  }
	  break;

	case CentredBrick:
	  {
	    int i, j, k;

	    QPainter paint( bgPixmap );
	    paint.setPen( Qt::white );
	    for ( i=k=0; i < (int)w; i+=20,k++ ) {
	      paint.drawLine( 0, i, w, i );
	      for (j=(k&1) * 20 + 10; j< (int)w; j+=40)
		paint.drawLine( j, i, j, i+20 );
	    }

	    bitBlt( bgPixmap, ( w - wpPixmap->width() ) / 2,
		    ( h - wpPixmap->height() ) / 2, wpPixmap, 0, 0,
		    wpPixmap->width(), wpPixmap->height() );
	  }
	  break;

	case CentredWarp:
	  {
	    int i;

	    QPainter paint( bgPixmap );
	    paint.setPen( Qt::white );
	    for ( i=0; i < (int)w; i+=8 )
	      paint.drawLine( i, 0, w - i, h );
	    for ( i=0; i < (int)h; i+=8 )
	      paint.drawLine( 0, i, w, h - i );

	    bitBlt( bgPixmap, ( w - wpPixmap->width() ) / 2,
		    ( h - wpPixmap->height() ) / 2, wpPixmap, 0, 0,
		    wpPixmap->width(), wpPixmap->height() );
	  }
	  break;
	}

      delete wpPixmap;
      wpPixmap = 0;
	
    }
}


void RootPixmap::generateBackground(double r, double  g, double b)
{
generateBackground(true,r,g,b);
}

/* This is the original code of Antonion
   The code I (CT) used to replace this is Mosfet's intensity effect.

   For some reason, when doing tests with kdelibs/kdetest/kcolortest, I can't 
   see significant delay differences.

   Anyways, for whatever reason, the KPixmapEffect::intensity method is *much*
   faster in real-work test with Konsole (I compared changing sizes of
   konsole by using Size menu)

void RootPixmap::shadePixmap(QPixmap *pm, double r, double  g, double b)
{
  QImage qimg=pm->convertToImage();
  uint *qptr=(uint *)qimg.bits();
  QRgb qrgb;
  int size=pm->width()*pm->height();
  for (int i=0;i<size; i++, qptr++)
  {
// I suspect the below code is faster as it doesn't triplicate casts, but it
//  seems experimental values give similar results. 
//	*qptr=qRgb(qRed(*(QRgb *)qptr)*r,qGreen(*(QRgb *)qptr)*g,qBlue(*(QRgb *)qptr)*b);
	qrgb=*(QRgb *)qptr;
	*qptr=qRgb((int)(qRed(qrgb)*r),(int)(qGreen(qrgb)*g),(int)(qBlue(qrgb)*b));

  }
  pm->convertFromImage(qimg);

}
*/

// new fading method - much faster (CT 02Aug1999)
void RootPixmap::shadePixmap(QPixmap *pm, double r, double  g, double b)
{
  //use Mosfet's channelIntensity effects
  QImage tmp = pm->convertToImage();
  KPixmapEffect::channelIntensity(tmp, (float)r, KPixmapEffect::Red,   false);
  KPixmapEffect::channelIntensity(tmp, (float)g, KPixmapEffect::Green, false);
  KPixmapEffect::channelIntensity(tmp, (float)b, KPixmapEffect::Blue,  false);
  pm->convertFromImage(tmp);
}

void RootPixmap::shadeColor(QColor *color, double r, double  g, double b)
{
//  QRgb qrgb=color->rgb()
  color->setRgb((int)(color->red()*r),(int)(color->green()*g),(int)(color->blue()*b));
}

QPixmap *RootPixmap::getPixmap(int x,int y,int w,int h)
{
#ifdef ROOTPIXMAP_FAST_RESIZE
w=bgPixmap->width()-x;
h=bgPixmap->height()-y;
#else
if (w>bgPixmap->width()-x) w=bgPixmap->width()-x;
if (h>bgPixmap->height()-y) h=bgPixmap->height()-y;
#endif
QPixmap *pm=new QPixmap(w,h,bgPixmap->depth());
bitBlt(pm,0,0,bgPixmap,x,y,w,h,Qt::CopyROP);
return pm;
}

QPixmap *RootPixmap::getPixmap()
{
return bgPixmap;
};

void RootPixmap::prepareBackground(double r, double g, double b, int num)
{
  if (num==-1) num=KWM::currentDesktop()-1;

  readSettings(num);
  generateBackground(true,r,g,b);
}

void RootPixmap::prepareBackground(int num)
{
  if (num==-1) num=KWM::currentDesktop()-1;

  readSettings(num);
  generateBackground();
}

void RootPixmap::setBackgroundPixmap(QWidget *w)
{
  QPoint topleft=w->mapToGlobal(QPoint(0,0));
  QPoint btmright=w->mapToGlobal(QPoint(w->width(),w->height()));
  QPixmap *tmpxpm=getPixmap(topleft.x(),topleft.y(),btmright.x()-topleft.x(),
							btmright.y()-topleft.y());
  w->setBackgroundPixmap(*tmpxpm);
  delete tmpxpm;
}
