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
/* -------------------------------------------------------------------------- */

class RootPixmap 
{

protected:

void readSettings(int num);
QPixmap *loadWallpaper(void);
void generateBackground(bool shade=false, double r=1.0, double g=1.0, double b=1.0);
void generateBackground(double r,double g, double b);
void shadePixmap(QPixmap *pm,double r,double g, double b); 

public:
RootPixmap();
~RootPixmap();

void prepareBackground(double r, double g, double b, int num=-1);
void prepareBackground(int num=-1);

int desktopNum(void) { return dsk; };

QPixmap *getPixmap(int x,int y,int w,int h);
QPixmap *getPixmap(); 

void setBackgroundPixmap(QWidget *w);

private:
  int   dsk; // Desktop for which we store the background
  uint pattern[8];
  QPixmap *bgPixmap;
  bool    applied;

  QString wallpaper;
  QColor  color1;
  QColor  color2;
  int     wpMode;
  int     gfMode;
  int     orMode;

  bool    hasPm;
  bool bUseWallpaper;

 enum { Tiled = 1,
	 Mirrored,
	 CenterTiled,
	 Centred,
	 CentredBrick,
	 CentredWarp,
	 CentredMaxpect,
	 SymmetricalTiled,
	 SymmetricalMirrored,
	 Scaled };
  enum { Flat = 1, Gradient, Pattern };
  enum { Portrait = 1, Landscape };
};
