/* ------------------------------------------------------------------------ */
/*                                                                          */
/* [TEWidget.cpp]        Terminal Emulation Widget                          */
/*                                                                          */
/* ------------------------------------------------------------------------ */
/*                                                                          */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>          */
/*                                                                          */
/* This file is part of Konsole - an X terminal for KDE                     */
/*                                                                          */
/* ------------------------------------------------------------------------ */

/*! \class TEWidget

    \brief Visible screen contents

   This class is responsible to map the `image' of a terminal emulation to the
   display. All the dependency of the emulation to a specific GUI or toolkit is
   localized here. Further, this widget has no knowledge about being part of an
   emulation, it simply work within the terminal emulation framework by exposing
   size and key events and by being ordered to show a new image.

   <ul>
   <li> The internal image has the size of the widget (evtl. rounded up)
   <li> The external image used in setImage can have any size.
   <li> (internally) the external image is simply copied to the internal
        when a setImage happens. During a resizeEvent no painting is done
        a paintEvent is expected to follow anyway.
   </ul>

   \sa TEScreen \sa Emulation
*/

/* FIXME:
   - 'image' may also be used uninitialized (it isn't in fact) in resizeEvent
   - 'font_a' not used in mouse events
   - add destructor
*/

/* TODO
   - evtl. be sensitive to `paletteChange' while using default colors.
   - set different 'rounding' styles? I.e. have a mode to show clipped chars?
*/

#include "config.h"
#include "TEWidget.h"
#include "session.h"

#include <qpainter.h>
#include <qclipboard.h>
#include <qstyle.h>
#include <qfile.h>
#include <qdragobject.h>
#include <qregexp.h>
#include <qlayout.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/stat.h>

#include <assert.h>

#include "TEWidget.moc"
#include <krun.h>
#include <kcursor.h>
#include <kdebug.h>
#include <klocale.h>
#include <knotifyclient.h>
#include <kglobalsettings.h>

#ifndef HERE
#define HERE printf("%s(%d): %s\n",__FILE__,__LINE__,__FUNCTION__)
#endif
#ifndef HCNT
#define HCNT(Name) // { static int cnt = 1; printf("%s(%d): %s %d\n",__FILE__,__LINE__,Name,cnt++); }
#endif

#ifndef loc
#define loc(X,Y) ((Y)*columns+(X))
#endif

//FIXME: the rim should normally be 1, 0 only when running in full screen mode.
#define rimX 0      // left/right rim width
#define rimY 0      // top/bottom rim high

#define SCRWIDTH 16 // width of the scrollbar

#define yMouseScroll 1
// scroll increment used when dragging selection at top/bottom of window.

// static
bool TEWidget::s_antialias = true;

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Colors                                     */
/*                                                                           */
/* ------------------------------------------------------------------------- */

//FIXME: the default color table is in session.C now.
//       We need a way to get rid of this one, here.
static const ColorEntry base_color_table[TABLE_COLORS] =
// The following are almost IBM standard color codes, with some slight
// gamma correction for the dim colors to compensate for bright X screens.
// It contains the 8 ansiterm/xterm colors in 2 intensities.
{
  // Fixme: could add faint colors here, also.
  // normal
  ColorEntry(QColor(0x00,0x00,0x00), 0, 0 ), ColorEntry( QColor(0xB2,0xB2,0xB2), 1, 0 ), // Dfore, Dback
  ColorEntry(QColor(0x00,0x00,0x00), 0, 0 ), ColorEntry( QColor(0xB2,0x18,0x18), 0, 0 ), // Black, Red
  ColorEntry(QColor(0x18,0xB2,0x18), 0, 0 ), ColorEntry( QColor(0xB2,0x68,0x18), 0, 0 ), // Green, Yellow
  ColorEntry(QColor(0x18,0x18,0xB2), 0, 0 ), ColorEntry( QColor(0xB2,0x18,0xB2), 0, 0 ), // Blue,  Magenta
  ColorEntry(QColor(0x18,0xB2,0xB2), 0, 0 ), ColorEntry( QColor(0xB2,0xB2,0xB2), 0, 0 ), // Cyan,  White
  // intensiv
  ColorEntry(QColor(0x00,0x00,0x00), 0, 1 ), ColorEntry( QColor(0xFF,0xFF,0xFF), 1, 0 ),
  ColorEntry(QColor(0x68,0x68,0x68), 0, 0 ), ColorEntry( QColor(0xFF,0x54,0x54), 0, 0 ),
  ColorEntry(QColor(0x54,0xFF,0x54), 0, 0 ), ColorEntry( QColor(0xFF,0xFF,0x54), 0, 0 ),
  ColorEntry(QColor(0x54,0x54,0xFF), 0, 0 ), ColorEntry( QColor(0xFF,0x54,0xFF), 0, 0 ),
  ColorEntry(QColor(0x54,0xFF,0xFF), 0, 0 ), ColorEntry( QColor(0xFF,0xFF,0xFF), 0, 0 )
};

/* Note that we use ANSI color order (bgr), while IBMPC color order is (rgb)

   Code        0       1       2       3       4       5       6       7
   ----------- ------- ------- ------- ------- ------- ------- ------- -------
   ANSI  (bgr) Black   Red     Green   Yellow  Blue    Magenta Cyan    White
   IBMPC (rgb) Black   Blue    Green   Cyan    Red     Magenta Yellow  White
*/

QColor TEWidget::getDefaultBackColor()
{
  return color_table[DEFAULT_BACK_COLOR].color;
}

const ColorEntry* TEWidget::getColorTable() const
{
  return color_table;
}

void TEWidget::setColorTable(const ColorEntry table[])
{
  for (int i = 0; i < TABLE_COLORS; i++) color_table[i] = table[i];
  const QPixmap* pm = backgroundPixmap();
  if (!pm) setBackgroundColor(color_table[DEFAULT_BACK_COLOR].color);
  update();
}

//FIXME: add backgroundPixmapChanged.

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                   Font                                    */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*
   The VT100 has 32 special graphical characters. The usual vt100 extended
   xterm fonts have these at 0x00..0x1f.

   QT's iso mapping leaves 0x00..0x7f without any changes. But the graphicals
   come in here as proper unicode characters.

   We treat non-iso10646 fonts as VT100 extended and do the requiered mapping
   from unicode to 0x00..0x1f. The remaining translation is then left to the
   QCodec.
*/

// assert for i in [0..31] : vt100extended(vt100_graphics[i]) == i.

unsigned short vt100_graphics[32] =
{ // 0/8     1/9    2/10    3/11    4/12    5/13    6/14    7/15
  0x0020, 0x25C6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0,
  0x00b1, 0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c,
  0xF800, 0xF801, 0x2500, 0xF803, 0xF804, 0x251c, 0x2524, 0x2534,
  0x252c, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00b7
};

static QChar vt100extended(QChar c)
{
  switch (c.unicode())
  {
    case 0x25c6 : return  1;
    case 0x2592 : return  2;
    case 0x2409 : return  3;
    case 0x240c : return  4;
    case 0x240d : return  5;
    case 0x240a : return  6;
    case 0x00b0 : return  7;
    case 0x00b1 : return  8;
    case 0x2424 : return  9;
    case 0x240b : return 10;
    case 0x2518 : return 11;
    case 0x2510 : return 12;
    case 0x250c : return 13;
    case 0x2514 : return 14;
    case 0x253c : return 15;
    case 0xf800 : return 16;
    case 0xf801 : return 17;
    case 0x2500 : return 18;
    case 0xf803 : return 19;
    case 0xf804 : return 20;
    case 0x251c : return 21;
    case 0x2524 : return 22;
    case 0x2534 : return 23;
    case 0x252c : return 24;
    case 0x2502 : return 25;
    case 0x2264 : return 26;
    case 0x2265 : return 27;
    case 0x03c0 : return 28;
    case 0x2260 : return 29;
    case 0x00a3 : return 30;
    case 0x00b7 : return 31;
  }
  return c;
}

static QChar identicalMap(QChar c)
{
  return c;
}

void TEWidget::fontChange(const QFont &)
{
  QFontMetrics fm(font());
  font_h = fm.height() + m_lineSpacing;
  int fw;
  font_w = 1;
  for(int i=0;i<128;i++) {
    if( isprint(i) && font_w < (fw = fm.width(i)))
      font_w = fw;
  }
  font_a = fm.ascent();
//printf("font: %s\n", font().toString().latin1());
//printf("fixed: %s\n", font().fixedPitch() ? "yes" : "no");
//printf("font_h: %d\n",font_h);
//printf("font_w: %d\n",font_w);
//printf("font_a: %d\n",font_a);
//printf("rawname: %s\n",font().rawName().ascii());
    
#if QT_VERSION < 300
  fontMap = strcmp(QFont::encodingName(font().charSet()).ascii(),"iso10646")
          ? vt100extended
          : identicalMap;
#else
#if defined(Q_CC_GNU)
#warning TODO: Review/fix vt100 extended font-mapping
#endif
  fontMap = identicalMap; 
#endif
  propagateSize();
  update();
}

void TEWidget::setVTFont(const QFont& f)
{
  QFont font = f;
  if (!s_antialias)
    font.setStyleStrategy( QFont::NoAntialias );
  QFrame::setFont(font);
  fontChange(font);
}

void TEWidget::setFont(const QFont &)
{
  // ignore font change request if not coming from konsole itself
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Constructor / Destructor                          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

TEWidget::TEWidget(QWidget *parent, const char *name)
:QFrame(parent,name)
,currentSession(0)
,font_h(1)
,font_w(1)
,font_a(1)
,lines(1)
,columns(1)
,image(0)
,resizing(false)
,actSel(0)
,word_selection_mode(false)
,line_selection_mode(false)
,preserve_line_breaks(true)
,scrollLoc(SCRNONE)
,word_characters(":@-./_~")
,bellMode(BELLSYSTEM)
,blinking(false)
,cursorBlinking(false)
,hasBlinkingCursor(false)
,ctrldrag(false)
,m_drop(0)
,possibleTripleClick(false)
,mResizeWidget(0)
,mResizeLabel(0)
,mResizeTimer(0)
,m_lineSpacing(0)
{
  // The offsets are not yet calculated. 
  // Do not calculate these too often to be more smoothly when resizing
  // konsole in opaque mode.
  bY = bX = 1;

  cb = QApplication::clipboard();
  QObject::connect( (QObject*)cb, SIGNAL(selectionChanged()),
                    this, SLOT(onClearSelection()) );

  scrollbar = new QScrollBar(this);
  scrollbar->setCursor( arrowCursor );
  connect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(scrollChanged(int)));

  blinkT   = new QTimer(this);
  connect(blinkT, SIGNAL(timeout()), this, SLOT(blinkEvent()));
  blinkCursorT   = new QTimer(this);
  connect(blinkCursorT, SIGNAL(timeout()), this, SLOT(blinkCursorEvent()));

  setMouseMarks(TRUE);
  setVTFont( QFont("fixed") );
  setColorTable(base_color_table); // init color table

  qApp->installEventFilter( this ); //FIXME: see below
  KCursor::setAutoHideCursor( this, true );

  // Init DnD ////////////////////////////////////////////////////////////////
  setAcceptDrops(true); // attempt
  dragInfo.state = diNone;

  setFocusPolicy( WheelFocus );
}

//FIXME: make proper destructor
// Here's a start (David)
TEWidget::~TEWidget()
{
  qApp->removeEventFilter( this );
  if (image) free(image);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Display Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*!
    attributed string draw primitive
*/

void TEWidget::drawAttrStr(QPainter &paint, QRect rect,
                           QString& str, ca attr, bool pm, bool clear)
{
  QColor fColor=(((attr.r & RE_CURSOR) && hasFocus() && (!hasBlinkingCursor || !cursorBlinking)) ? color_table[attr.b].color : color_table[attr.f].color);
  QColor bColor=(((attr.r & RE_CURSOR) && hasFocus() && (!hasBlinkingCursor || !cursorBlinking)) ? color_table[attr.f].color : color_table[attr.b].color);

  if (attr.r & RE_CURSOR)
    cursorRect = rect;

  if (pm && color_table[attr.b].transparent && (!(attr.r & RE_CURSOR) || cursorBlinking))
  {
    paint.setBackgroundMode( TransparentMode );
    if (clear) erase(rect);
  }
  else
  {
    if (blinking)
      paint.fillRect(rect, bColor);
    else
    {
      paint.setBackgroundMode( OpaqueMode );
      paint.setBackgroundColor( bColor );
    }
  }
  if (!(blinking && (attr.r & RE_BLINK)))
  {
    if ((attr.r && RE_CURSOR) && cursorBlinking)
      erase(rect);
    paint.setPen(fColor);
    paint.drawText(rect.x(),rect.y()+font_a, str);
    if ((attr.r & RE_UNDERLINE) || color_table[attr.f].bold)
    {
      paint.setClipRect(rect);
      if (color_table[attr.f].bold)
      {
        paint.setBackgroundMode( TransparentMode );
        paint.drawText(rect.x()+1,rect.y()+font_a, str); // second stroke
      }
      if (attr.r & RE_UNDERLINE)
        paint.drawLine(rect.left(), rect.y()+font_a+1,
                       rect.right(),rect.y()+font_a+1 );
      paint.setClipping(FALSE);
    }
  }
  if ((attr.r & RE_CURSOR) && !hasFocus()) {
    if (pm && color_table[attr.b].transparent)
    {
      erase(rect);
      paint.setBackgroundMode( TransparentMode );
      paint.drawText(rect.x(),rect.y()+font_a, str);
    }
    paint.setClipRect(rect);
    paint.drawRect(rect.x(),rect.y(),rect.width(),rect.height()-m_lineSpacing);
    paint.setClipping(FALSE);
  }
}

/*!
    Set XIM Position
*/
void TEWidget::setCursorPos(const int curx, const int cury)
{
    QPoint tL  = contentsRect().topLeft();
    int    tLx = tL.x();
    int    tLy = tL.y();

    int xpos, ypos;
    ypos = bY + tLy + font_h*(cury-1) + font_a;
    xpos = bX + tLx + font_w*curx;
    setMicroFocusHint(xpos, ypos, 0, font_h);
    // fprintf(stderr, "x/y = %d/%d\txpos/ypos = %d/%d\n", curx, cury, xpos, ypos);
}

/*!
    The image can only be set completely.

    The size of the new image may or may not match the size of the widget.
*/

void TEWidget::setImage(const ca* const newimg, int lines, int columns)
{ int y,x,len;
  const QPixmap* pm = backgroundPixmap();
  QPainter paint;
  setUpdatesEnabled(FALSE);
  paint.begin( this );
HCNT("setImage");

  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();
  hasBlinker = FALSE;

  int cf  = -1; // undefined
  int cb  = -1; // undefined
  int cr  = -1; // undefined

  int lins = QMIN(this->lines,  QMAX(0,lines  ));
  int cols = QMIN(this->columns,QMAX(0,columns));
  QChar *disstrU = new QChar[cols];

//{ static int cnt = 0; printf("setImage %d\n",cnt++); }
  for (y = 0; y < lins; y++)
  {
    const ca*       lcl = &image[y*this->columns];
    const ca* const ext = &newimg[y*columns];
    if (!resizing) // not while resizing, we're expecting a paintEvent
    for (x = 0; x < cols; x++)
    {
      hasBlinker |= (ext[x].r & RE_BLINK);
      if (ext[x] != lcl[x])
      {
        Q_UINT16 c = ext[x+0].c;
        if ( !c )
            continue;
        int p = 0;
        disstrU[p++] = fontMap(c);
        cr = ext[x].r;
        cb = ext[x].b;
        if (ext[x].f != cf) cf = ext[x].f;
        int lln = cols - x;
        for (len = 1; len < lln; len++)
        {
          c = ext[x+len].c;
          if (!c)
            continue; // Skip trailing part of multi-col chars.

          if (ext[x+len].f != cf || ext[x+len].b != cb || ext[x+len].r != cr ||
              ext[x+len] == lcl[x+len] )
            break;

          disstrU[p++] = fontMap(c);
        }

        QString unistr(disstrU, p);
        drawAttrStr(paint,
                    QRect(bX+tLx+font_w*x,bY+tLy+font_h*y,font_w*len,font_h),
                    unistr, ext[x], pm != NULL, true);
        x += len - 1;
      }
    }
    // finally, make `image' become `newimg'.
    memcpy((void*)lcl,(const void*)ext,cols*sizeof(ca));
  }
  drawFrame( &paint );
  paint.end();
  setUpdatesEnabled(TRUE);
  if ( hasBlinker && !blinkT->isActive()) blinkT->start(1000); // 1000 ms
  if (!hasBlinker && blinkT->isActive()) { blinkT->stop(); blinking = FALSE; }
  delete [] disstrU;

  if (resizing && terminalSizeHint)
  {
     if (!mResizeWidget)
     {
        mResizeWidget = new QFrame(this);
        QFont f = mResizeWidget->font();
        f.setPointSize(f.pointSize()*2);
        f.setBold(true);
        mResizeWidget->setFont(f);
        mResizeWidget->setFrameShape((QFrame::Shape) (QFrame::Box|QFrame::Raised));
        mResizeWidget->setMidLineWidth(4);
        QBoxLayout *l = new QVBoxLayout( mResizeWidget, 10);
        mResizeLabel = new QLabel(i18n("Size: XXX x XXX"), mResizeWidget);
        l->addWidget(mResizeLabel, 1, AlignCenter);
        mResizeWidget->setMinimumWidth(mResizeLabel->fontMetrics().width(i18n("Size: XXX x XXX"))+20);
        mResizeWidget->setMinimumHeight(mResizeLabel->sizeHint().height()+20);
        mResizeTimer = new QTimer(this);
        connect(mResizeTimer, SIGNAL(timeout()), mResizeWidget, SLOT(hide()));
     }
     QString sizeStr = i18n("Size: %1 x %2").arg(columns).arg(lines);
     mResizeLabel->setText(sizeStr);
     mResizeWidget->move((width()-mResizeWidget->width())/2,
                         (height()-mResizeWidget->height())/2);
     mResizeWidget->show();
     mResizeTimer->start(1000, true);
  }
}

void TEWidget::setBlinkingCursor(bool blink)
{
  hasBlinkingCursor=blink;
  if (blink && !blinkCursorT->isActive()) blinkCursorT->start(1000);
  if (!blink && blinkCursorT->isActive()) {
    blinkCursorT->stop();
    if (cursorBlinking)
      blinkCursorEvent();
    else
      cursorBlinking = FALSE;
  }
}

// paint Event ////////////////////////////////////////////////////

/*!
    The difference of this routine vs. the `setImage' is,
    that the drawing does not include a difference analysis
    between the old and the new image. Instead, the internal
    image is used and the painting bound by the PaintEvent box.
*/

void TEWidget::paintEvent( QPaintEvent* pe )
{

//{ static int cnt = 0; printf("paint %d\n",cnt++); }
  const QPixmap* pm = backgroundPixmap();
  QPainter paint;
  setUpdatesEnabled(FALSE);
  paint.begin( this );
  paint.setBackgroundMode( TransparentMode );
HCNT("paintEvent");

  // Note that the actual widget size can be slightly larger
  // that the image (the size is truncated towards the smaller
  // number of characters in `resizeEvent'. The paint rectangle
  // can thus be larger than the image, but less then the size
  // of one character.

  QRect rect = pe->rect().intersect(contentsRect());

  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();

  int lux = QMIN(columns-1, QMAX(0,(rect.left()   - tLx - bX ) / font_w));
  int luy = QMIN(lines-1,   QMAX(0,(rect.top()    - tLy - bY  ) / font_h));
  int rlx = QMIN(columns-1, QMAX(0,(rect.right()  - tLx - bX ) / font_w));
  int rly = QMIN(lines-1,   QMAX(0,(rect.bottom() - tLy - bY  ) / font_h));

  //  if (pm != NULL && color_table[image->b].transparent)
  //  erase(rect);
  // BL: I have no idea why we need this, and it breaks the refresh.

  QChar *disstrU = new QChar[columns];
  for (int y = luy; y <= rly; y++)
  {
    Q_UINT16 c = image[loc(lux,y)].c;
    int x = lux;
    if(!c && x)
      x--; // Search for start of multi-col char
    for (; x <= rlx; x++)
    {
      int len = 1;
      int p = 0;
      c = image[loc(x,y)].c;
      if (c)
         disstrU[p++] = fontMap(c);
      int cf = image[loc(x,y)].f;
      int cb = image[loc(x,y)].b;
      int cr = image[loc(x,y)].r;
      while (x+len <= rlx &&
             image[loc(x+len,y)].f == cf &&
             image[loc(x+len,y)].b == cb &&
             image[loc(x+len,y)].r == cr )
      {
        c = image[loc(x+len,y)].c;
        if (c)
          disstrU[p++] = fontMap(c);
        len++;
      }
      if ((x+len < columns) && (!image[loc(x+len,y)].c))
        len++; // Adjust for trailing part of multi-column char

      QString unistr(disstrU,p);
      drawAttrStr(paint,
                QRect(bX+tLx+font_w*x,bY+tLy+font_h*y,font_w*len,font_h),
                unistr, image[loc(x,y)], pm != NULL, false);
      x += len - 1;
    }
  }
  delete [] disstrU;
  drawFrame( &paint );
  paint.end();
  setUpdatesEnabled(TRUE);
}

void TEWidget::blinkEvent()
{
  blinking = !blinking;
  repaint(FALSE);
}

void TEWidget::blinkCursorEvent()
{
  cursorBlinking = !cursorBlinking;
  repaint(cursorRect, FALSE);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                  Resizing                                 */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEWidget::resizeEvent(QResizeEvent* ev)
{
  //printf("resize: %d,%d\n",ev->size().width(),ev->size().height());
  //printf("approx: %d,%d\n",ev->size().width()/font_w,ev->size().height()/font_h);
  //printf("leaves: %d,%d\n",ev->size().width()%font_w,ev->size().height()%font_h);
  //printf("curren: %d,%d\n",width(),height());
HCNT("resizeEvent");

  // see comment in `paintEvent' concerning the rounding.
  //FIXME: could make a routine here; check width(),height()
  assert(ev->size().width() == width());
  assert(ev->size().height() == height());

  propagateSize();
}

void TEWidget::propagateSize()
{
  ca* oldimg = image;
  int oldlin = lines;
  int oldcol = columns;
  makeImage();
  // we copy the old image to reduce flicker
  int lins = QMIN(oldlin,lines);
  int cols = QMIN(oldcol,columns);
  if (oldimg)
  {
    for (int lin = 0; lin < lins; lin++)
      memcpy((void*)&image[columns*lin],
             (void*)&oldimg[oldcol*lin],cols*sizeof(ca));
    free(oldimg); //FIXME: try new,delete
  }
  else
    clearImage();

  //NOTE: control flows from the back through the chest right into the eye.
  //      `emu' will call back via `setImage'.

  resizing = TRUE;
  emit changedImageSizeSignal(lines, columns); // expose resizeEvent
  resizing = FALSE;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Scrollbar                                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEWidget::scrollChanged(int)
{
  emit changedHistoryCursor(scrollbar->value()); //expose
}

void TEWidget::setScroll(int cursor, int slines)
{
  //kdDebug(1211)<<"TEWidget::setScroll() disconnect()"<<endl;
  disconnect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(scrollChanged(int)));
  //kdDebug(1211)<<"TEWidget::setScroll() setRange()"<<endl;
  scrollbar->setRange(0,slines);
  //kdDebug(1211)<<"TEWidget::setScroll() setSteps()"<<endl;
  scrollbar->setSteps(1,lines);
  scrollbar->setValue(cursor);
  connect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(scrollChanged(int)));
  //kdDebug(1211)<<"TEWidget::setScroll() done"<<endl;
}

void TEWidget::setScrollbarLocation(int loc)
{
  if (scrollLoc == loc) return; // quickly
  bY = bX = 1;
  scrollLoc = loc;
  propagateSize();
  update();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                   Mouse                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*!
    Three different operations can be performed using the mouse, and the
    routines in this section serve all of them:

    1) The press/release events are exposed to the application
    2) Marking (press and move left button) and Pasting (press middle button)
    3) The right mouse button is used from the configuration menu

    NOTE: During the marking process we attempt to keep the cursor within
    the bounds of the text as being displayed by setting the mouse position
    whenever the mouse has left the text area.

    Two reasons to do so:
    1) QT does not allow the `grabMouse' to confine-to the TEWidget.
       Thus a `XGrapPointer' would have to be used instead.
    2) Even if so, this would not help too much, since the text area
       of the TEWidget is normally not identical with it's bounds.

    The disadvantage of the current handling is, that the mouse can visibly
    leave the bounds of the widget and is then moved back. Because of the
    current construction, and the reasons mentioned above, we cannot do better
    without changing the overall construction.
*/

/*!
*/

void TEWidget::mousePressEvent(QMouseEvent* ev)
{
//printf("press [%d,%d] %d\n",ev->x()/font_w,ev->y()/font_h,ev->button());

  if ( possibleTripleClick && (ev->button()==LeftButton) ) {
    mouseTripleClickEvent(ev);
    return;
  }

  if ( !contentsRect().contains(ev->pos()) ) return;
  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();

  line_selection_mode = FALSE;
  word_selection_mode = FALSE;

  QPoint pos = QPoint((ev->x()-tLx-bX)/font_w,(ev->y()-tLy-bY)/font_h);

//printf("press top left [%d,%d] by=%d\n",tLx,tLy, bY);
  if ( ev->button() == LeftButton)
  {
    emit isBusySelecting(true); // Keep it steady...
    // Drag only when the Control key is hold
    bool selected = false;
    // The receiver of the testIsSelected() signal will adjust 
    // 'selected' accordingly.
    emit testIsSelected(pos.x(), pos.y(), selected);
    if ((!ctrldrag || ev->state() & ControlButton) && selected ) {
      // The user clicked inside selected text
      dragInfo.state = diPending;
      dragInfo.start = ev->pos();
    } 
    else {
      // No reason to ever start a drag event
      dragInfo.state = diNone;

      preserve_line_breaks = !( ev->state() & ControlButton );

      if (mouse_marks || (ev->state() & ShiftButton))
      {
        emit clearSelectionSignal();
        iPntSel = pntSel = pos;
        actSel = 1; // left mouse button pressed but nothing selected yet.
        grabMouse(   /*crossCursor*/  ); // handle with care!	
      }
      else
      {
        emit mouseSignal( 0, pos.x() + 1, pos.y() + 1 ); // left button
      }
    }
  }
  else if ( ev->button() == MidButton )
  {
    if (mouse_marks)
      emitSelection(true,ev->state() & ControlButton);
    else
      emit mouseSignal( 1, (ev->x()-tLx-bX)/font_w +1, (ev->y()-tLy-bY)/font_h +1 );
  }
  else if ( ev->button() == RightButton )
  {
    if (mouse_marks || (ev->state() & ShiftButton))
      emit configureRequest( this, ev->state()&(ShiftButton|ControlButton), ev->x(), ev->y() );
    else
      emit mouseSignal( 2, (ev->x()-tLx-bX)/font_w +1, (ev->y()-tLy-bY)/font_h +1 );
  }
}

void TEWidget::mouseMoveEvent(QMouseEvent* ev)
{
  // for auto-hiding the cursor, we need mouseTracking
  if (ev->state() == NoButton ) return;

  if (dragInfo.state == diPending) {
    // we had a mouse down, but haven't confirmed a drag yet
    // if the mouse has moved sufficiently, we will confirm

   int distance = KGlobalSettings::dndEventDelay();
   if ( ev->x() > dragInfo.start.x() + distance || ev->x() < dragInfo.start.x() - distance ||
        ev->y() > dragInfo.start.y() + distance || ev->y() < dragInfo.start.y() - distance) {
      // we've left the drag square, we can start a real drag operation now
      emit isBusySelecting(false); // Ok.. we can breath again.
      emit clearSelectionSignal();
      doDrag();
    }
    return;
  } else if (dragInfo.state == diDragging) {
    // this isn't technically needed because mouseMoveEvent is suppressed during
    // Qt drag operations, replaced by dragMoveEvent
    return;
  }

  if (actSel == 0) return;

 // don't extend selection while pasting
  if (ev->state() & MidButton) return;

  //if ( !contentsRect().contains(ev->pos()) ) return;
  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();
  int    scroll = scrollbar->value();

  // we're in the process of moving the mouse with the left button pressed
  // the mouse cursor will kept catched within the bounds of the text in
  // this widget.

  // Adjust position within text area bounds. See FIXME above.
  QPoint pos = ev->pos();
  if ( pos.x() < tLx+bX )                  pos.setX( tLx+bX );
  if ( pos.x() > tLx+bX+columns*font_w-1 ) pos.setX( tLx+bX+columns*font_w );
  if ( pos.y() < tLy+bY )                   pos.setY( tLy+bY );
  if ( pos.y() > tLy+bY+lines*font_h-1 )    pos.setY( tLy+bY+lines*font_h-1 );
  // check if we produce a mouse move event by this
  if ( pos != ev->pos() ) cursor().setPos(mapToGlobal(pos));

  if ( pos.y() == tLy+bY+lines*font_h-1 )
  {
    scrollbar->setValue(scrollbar->value()+yMouseScroll); // scrollforward
  }
  if ( pos.y() == tLy+bY )
  {
    scrollbar->setValue(scrollbar->value()-yMouseScroll); // scrollback
  }

  QPoint here = QPoint((pos.x()-tLx-bX)/font_w,(pos.y()-tLy-bY)/font_h);
  QPoint ohere;
  bool swapping = FALSE;

  if ( word_selection_mode )
  {
    // Extend to word boundaries
    int i;
    int selClass;

    bool left_not_right = ( here.y() < iPntSel.y() ||
	   here.y() == iPntSel.y() && here.x() < iPntSel.x() );
    bool old_left_not_right = ( pntSel.y() < iPntSel.y() ||
	   pntSel.y() == iPntSel.y() && pntSel.x() < iPntSel.x() );
    swapping = left_not_right != old_left_not_right;

    // Find left (left_not_right ? from here : from start)
    QPoint left = left_not_right ? here : iPntSel;
    i = loc(left.x(),left.y());
    selClass = charClass(image[i].c);
    while ( ((left.x()>0) || (left.y()>0 && m_line_wrapped[left.y()-1])) && charClass(image[i-1].c) == selClass )
    { i--; if (left.x()>0) left.rx()--; else {left.rx()=columns-1; left.ry()--;} }

    // Find left (left_not_right ? from start : from here)
    QPoint right = left_not_right ? iPntSel : here;
    i = loc(right.x(),right.y());
    selClass = charClass(image[i].c);
    while( ((right.x()<columns-1) || (right.y()<lines-1 && m_line_wrapped[right.y()])) && charClass(image[i+1].c) == selClass )
    { i++; if (right.x()<columns-1) right.rx()++; else {right.rx()=0; right.ry()++; } }

    // Pick which is start (ohere) and which is extension (here)
    if ( left_not_right )
    {
      here = left; ohere = right;
    }
    else
    {
      here = right; ohere = left;
    }
  }

  if ( line_selection_mode )
  {
    // Extend to complete line
    bool above_not_below = ( here.y() < iPntSel.y() );
    bool old_above_not_below = ( pntSel.y() < iPntSel.y() );
    swapping = above_not_below != old_above_not_below;

    QPoint above = above_not_below ? here : iPntSel;
    QPoint below = above_not_below ? iPntSel : here;
    
    while (above.y()>0 && m_line_wrapped[above.y()-1])
      above.ry()--;
    while (below.y()<lines-1 && m_line_wrapped[below.y()])
      below.ry()++;

    above.setX(0);
    below.setX(columns-1);

    // Pick which is start (ohere) and which is extension (here)
    if ( above_not_below )
    {
      here = above; ohere = below;
    }
    else
    {
      here = below; ohere = above;
    }
  }

  if ( !word_selection_mode && !line_selection_mode )
  {
    int i;
    int selClass;

    bool left_not_right = ( here.y() < iPntSel.y() ||
	   here.y() == iPntSel.y() && here.x() < iPntSel.x() );
    bool old_left_not_right = ( pntSel.y() < iPntSel.y() ||
	   pntSel.y() == iPntSel.y() && pntSel.x() < iPntSel.x() );
    swapping = left_not_right != old_left_not_right;

    // Find left (left_not_right ? from here : from start)
    QPoint left = left_not_right ? here : iPntSel;

    // Find left (left_not_right ? from start : from here)
    QPoint right = left_not_right ? iPntSel : here;
    i = loc(right.x(),right.y());
    selClass = charClass(image[i].c);
    if (selClass == ' ')
    {
       while ( right.x() < columns-1 && charClass(image[i+1].c) == selClass && !m_line_wrapped[right.y()])
       { i++; right.rx()++; }
       if (right.x() < columns-1) right = left_not_right ? iPntSel : here;
    }

    // Pick which is start (ohere) and which is extension (here)
    if ( left_not_right )
    {
      here = left; ohere = right;
    }
    else
    {
      here = right; ohere = left;
    }

  }

  if (here == pntSel && scroll == scrollbar->value()) return; // not moved

  if ( word_selection_mode || line_selection_mode ) {
    if ( actSel < 2 || swapping ) {
      emit beginSelectionSignal( ohere.x(), ohere.y() );
    }
  } else if ( actSel < 2 ) {
    emit beginSelectionSignal( pntSel.x(), pntSel.y() );
  }

  actSel = 2; // within selection
  pntSel = here;
  emit extendSelectionSignal( here.x(), here.y() );
}

void TEWidget::mouseReleaseEvent(QMouseEvent* ev)
{
//printf("release [%d,%d] %d\n",ev->x()/font_w,ev->y()/font_h,ev->button());
  if ( ev->button() == LeftButton)
  {
    emit isBusySelecting(false); // Ok.. we can breath again.
    if(dragInfo.state == diPending)
    {
      // We had a drag event pending but never confirmed.  Kill selection
      emit clearSelectionSignal();
    }
    else
    {
      if ( actSel > 1 ) 
          emit endSelectionSignal(preserve_line_breaks);
      actSel = 0;

      //FIXME: emits a release event even if the mouse is
      //       outside the range. The procedure used in `mouseMoveEvent'
      //       applies here, too.

      QPoint tL  = contentsRect().topLeft();
      int    tLx = tL.x();
      int    tLy = tL.y();

      if (!mouse_marks && !(ev->state() & ShiftButton))
        emit mouseSignal( 3, // release
                        (ev->x()-tLx-bX)/font_w + 1,
                        (ev->y()-tLy-bY)/font_h + 1 );
      releaseMouse();
    }
    dragInfo.state = diNone;
  }
  if ( !mouse_marks && ((ev->button() == RightButton && !(ev->state() & ShiftButton))
                        || ev->button() == MidButton) ) {
    QPoint tL  = contentsRect().topLeft();
    int    tLx = tL.x();
    int    tLy = tL.y();

    emit mouseSignal( 3, (ev->x()-tLx-bX)/font_w +1, (ev->y()-tLy-bY)/font_h +1 );
    releaseMouse();
  }
}

void TEWidget::mouseDoubleClickEvent(QMouseEvent* ev)
{
  if ( ev->button() != LeftButton) return;

  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();
  QPoint pos = QPoint((ev->x()-tLx-bX)/font_w,(ev->y()-tLy-bY)/font_h);

  // pass on double click as two clicks.
  if (!mouse_marks && !(ev->state() & ShiftButton))
  {
    // Send just _ONE_ click event, since the first click of the double click
    // was already sent by the click handler!
    emit mouseSignal( 0, pos.x()+1, pos.y()+1 ); // left button
    return;
  }


  emit clearSelectionSignal();
  QPoint bgnSel = pos;
  QPoint endSel = pos;
  int i = loc(bgnSel.x(),bgnSel.y());
  iPntSel = bgnSel;

  word_selection_mode = TRUE;

  // find word boundaries...
  int selClass = charClass(image[i].c);
  {
    // set the start...
     int x = bgnSel.x();
     while ( ((x>0) || (bgnSel.y()>0 && m_line_wrapped[bgnSel.y()-1])) && charClass(image[i-1].c) == selClass )
     { i--; if (x>0) x--; else {x=columns-1; bgnSel.ry()--;} }
     bgnSel.setX(x);
     emit beginSelectionSignal( bgnSel.x(), bgnSel.y() );

     // set the end...
     i = loc( endSel.x(), endSel.y() );
     x = endSel.x();
     while( ((x<columns-1) || (endSel.y()<lines-1 && m_line_wrapped[endSel.y()])) && charClass(image[i+1].c) == selClass )
     { i++; if (x<columns-1) x++; else {x=0; endSel.ry()++; } }
     endSel.setX(x);
     actSel = 2; // within selection
     emit extendSelectionSignal( endSel.x(), endSel.y() );
     emit endSelectionSignal(preserve_line_breaks);
   }

  possibleTripleClick=true;
  QTimer::singleShot(QApplication::doubleClickInterval(),this,SLOT(tripleClickTimeout())); 
}

void TEWidget::tripleClickTimeout()
{
  possibleTripleClick=false;
}

void TEWidget::mouseTripleClickEvent(QMouseEvent* ev)
{
  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();
  iPntSel = QPoint((ev->x()-tLx-bX)/font_w,(ev->y()-tLy-bY)/font_h);

  emit clearSelectionSignal();

  line_selection_mode = TRUE;
  word_selection_mode = FALSE;

  actSel = 2; // within selection

  while (iPntSel.y()>0 && m_line_wrapped[iPntSel.y()-1])
    iPntSel.ry()--;
  emit beginSelectionSignal( 0, iPntSel.y() );

  while (iPntSel.y()<lines-1 && m_line_wrapped[iPntSel.y()])
    iPntSel.ry()++;
  emit extendSelectionSignal( 0, iPntSel.y()+1 );

  emit endSelectionSignal(preserve_line_breaks);
}

void TEWidget::focusInEvent( QFocusEvent * )
{
  repaint(cursorRect, true);  // *do* erase area, to get rid of the
                              // hollow cursor rectangle.
}


void TEWidget::focusOutEvent( QFocusEvent * )
{
  repaint(cursorRect, false);  // don't erase area
}

bool TEWidget::focusNextPrevChild( bool next )
{
  if (next)
    return false; // This disables changing the active part in konqueror
                  // when pressing Tab
  return QFrame::focusNextPrevChild( next );
}


int TEWidget::charClass(UINT16 ch) const
{
    QChar qch=QChar(ch);
    if ( qch.isSpace() ) return ' ';

    if ( qch.isLetterOrNumber() || word_characters.contains(qch, FALSE) )
    return 'a';

    // Everything else is weird
    return 1;
}

void TEWidget::setWordCharacters(QString wc)
{
	word_characters = wc;
}

void TEWidget::setMouseMarks(bool on)
{
  mouse_marks = on;
  setCursor( mouse_marks ? ibeamCursor : arrowCursor );
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               Clipboard                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#undef KeyPress

void TEWidget::emitText(QString text)
{
  if (!text.isEmpty()) {
    QKeyEvent e(QEvent::KeyPress, 0,-1,0, text);
    emit keyPressedSignal(&e); // expose as a big fat keypress event
  }
}

void TEWidget::emitSelection(bool useXselection,bool appendReturn)
// Paste Clipboard by simulating keypress events
{
  QApplication::clipboard()->setSelectionMode( useXselection );
  QString text = QApplication::clipboard()->text();
  if(appendReturn)  
    text.append("\r");
  if ( ! text.isEmpty() )
  {
    text.replace(QRegExp("\n"), "\r");
    QKeyEvent e(QEvent::KeyPress, 0,-1,0, text);
    emit keyPressedSignal(&e); // expose as a big fat keypress event
    emit clearSelectionSignal();
  }
  QApplication::clipboard()->setSelectionMode( false );
}

void TEWidget::setSelection(const QString& t)
{
  // Disconnect signal while WE set the clipboard
  QClipboard *cb = QApplication::clipboard();
  QObject::disconnect( cb, SIGNAL(selectionChanged()),
                     this, SLOT(onClearSelection()) );

  cb->setSelectionMode( true );
  QApplication::clipboard()->setText(t);
  cb->setSelectionMode( false );
  QApplication::clipboard()->setText(t);

  QObject::connect( cb, SIGNAL(selectionChanged()),
                     this, SLOT(onClearSelection()) );
}

void TEWidget::pasteClipboard()
{
  emitSelection(false,false);
}

void TEWidget::onClearSelection()
{
  emit clearSelectionSignal();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Keyboard                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

//FIXME: an `eventFilter' has been installed instead of a `keyPressEvent'
//       due to a bug in `QT' or the ignorance of the author to prevent
//       repaint events being emitted to the screen whenever one leaves
//       or reenters the screen to/from another application.
//
//   Troll says one needs to change focusInEvent() and focusOutEvent(),
//   which would also let you have an in-focus cursor and an out-focus
//   cursor like xterm does.

// for the auto-hide cursor feature, I added empty focusInEvent() and
// focusOutEvent() so that update() isn't called.
// For auto-hide, we need to get keypress-events, but we only get them when
// we have focus.

void TEWidget::doScroll(int lines)
{
  scrollbar->setValue(scrollbar->value()+lines);
}

bool TEWidget::eventFilter( QObject *obj, QEvent *e )
{
  if ( (e->type() == QEvent::Accel ||
       e->type() == QEvent::AccelAvailable ) && qApp->focusWidget() == this )
  {
      static_cast<QKeyEvent *>( e )->ignore();
      return true;
  }
  if ( obj != this /* when embedded */ && obj != parent() /* when standalone */ )
      return FALSE; // not us
  if ( e->type() == QEvent::Wheel)
  {
    QApplication::sendEvent(scrollbar, e);
  }
  if ( e->type() == QEvent::KeyPress )
  {
    QKeyEvent* ke = (QKeyEvent*)e;

    actSel=0; // Key stroke implies a screen update, so TEWidget won't
              // know where the current selection is.

    if (hasBlinkingCursor) {
      blinkCursorT->start(1000);
      if (cursorBlinking)
        blinkCursorEvent();
      else
        cursorBlinking = FALSE;
    }

    emit keyPressedSignal(ke); // expose
#if QT_VERSION < 300
    return false;               // accept event
#else
    // in Qt2 when key events were propagated up the tree 
    // (unhandled? -> parent widget) they passed the event filter only once at
    // the beginning. in qt3 this has changed, that is, the event filter is 
    // called each time the event is sent (see loop in QApplication::notify,
    // when internalNotify() is called for KeyPress, whereas internalNotify
    // activates also the global event filter) . That's why we stop propagation
    // here.
    return true;
#endif
  }
  if ( e->type() == QEvent::Enter )
  {
    QObject::disconnect( (QObject*)cb, SIGNAL(dataChanged()),
      this, SLOT(onClearSelection()) );
  }
  if ( e->type() == QEvent::Leave )
  {
    QObject::connect( (QObject*)cb, SIGNAL(dataChanged()),
      this, SLOT(onClearSelection()) );
  }
  return QFrame::eventFilter( obj, e );
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                  Frame                                    */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEWidget::frameChanged()
{
  propagateSize();
  update();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                   Sound                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEWidget::setBellMode(int mode)
{
  bellMode=mode;
}

void TEWidget::Bell()
{
  if (bellMode==BELLSYSTEM)
    KNotifyClient::beep();
  if (bellMode==BELLVISUAL) {
    swapColorTable();
    QTimer::singleShot(200,this,SLOT(swapColorTable()));
  }
}

void TEWidget::swapColorTable()
{
  ColorEntry color = color_table[1];
  color_table[1]=color_table[0];
  color_table[0]= color;
  update();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                 Auxiluary                                 */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEWidget::clearImage()
// initialize the image
// for internal use only
{
  for (int y = 0; y < lines; y++)
  for (int x = 0; x < columns; x++)
  {
    image[loc(x,y)].c = 0xff; //' ';
    image[loc(x,y)].f = 0xff; //DEFAULT_FORE_COLOR;
    image[loc(x,y)].b = 0xff; //DEFAULT_BACK_COLOR;
    image[loc(x,y)].r = 0xff; //DEFAULT_RENDITION;
  }
}

// Create Image ///////////////////////////////////////////////////////

void TEWidget::calcGeometry()
{
  //FIXME: set rimX == rimY == 0 when running in full screen mode.

  scrollbar->resize(QApplication::style().pixelMetric(QStyle::PM_ScrollBarExtent),
                    contentsRect().height());
  switch(scrollLoc)
  {
    case SCRNONE :
     bX = 1;
     columns = ( contentsRect().width() - 2 * rimX ) / font_w;
     scrollbar->hide();
     break;
    case SCRLEFT :
     bX = 1+scrollbar->width();
     columns = ( contentsRect().width() - 2 * rimX - scrollbar->width()) / font_w;
     scrollbar->move(contentsRect().topLeft());
     scrollbar->show();
     break;
    case SCRRIGHT:
     bX = 1;
     columns = ( contentsRect().width()  - 2 * rimX - scrollbar->width()) / font_w;
     scrollbar->move(contentsRect().topRight() - QPoint(scrollbar->width()-1,0));
     scrollbar->show();
     break;
  }
  //FIXME: support 'rounding' styles
  lines   = ( contentsRect().height() - 2 * rimY  ) / font_h;
}

void TEWidget::makeImage()
//FIXME: rename 'calcGeometry?
{
  calcGeometry();
  image = (ca*) malloc(lines*columns*sizeof(ca));
  clearImage();
}

// calculate the needed size
QSize TEWidget::calcSize(int cols, int lins) const
{
  int frw = width() - contentsRect().width();
  int frh = height() - contentsRect().height();
  int scw = (scrollLoc==SCRNONE?0:scrollbar->width());
  return QSize( font_w*cols + 2*rimX + frw + scw + 2, font_h*lins + 2*rimY + frh + 2 );
}

QSize TEWidget::sizeHint() const
{
   return size();
}

void TEWidget::styleChange(QStyle &)
{
    propagateSize();
}


/* --------------------------------------------------------------------- */
/*                                                                       */
/* Drag & Drop                                                           */
/*                                                                       */
/* --------------------------------------------------------------------- */

void TEWidget::dragEnterEvent(QDragEnterEvent* e)
{
  e->accept(QTextDrag::canDecode(e) ||
      QUriDrag::canDecode(e));
}

void TEWidget::dropEvent(QDropEvent* event)
{
   if (m_drop==0)
   {
      m_drop = new KPopupMenu(this);
      m_drop->insertItem( i18n("Paste"), 0);
      m_drop->insertItem( i18n("cd"),    1);
      connect(m_drop, SIGNAL(activated(int)), SLOT(drop_menu_activated(int)));
   };
    // The current behaviour when url(s) are dropped is
    // * if there is only ONE url and if it's a LOCAL one, ask for paste or cd
    // * in all other cases, just paste
    //   (for non-local ones, or for a list of URLs, 'cd' is nonsense)
  QStrList strlist;
  int file_count = 0;
  dropText = "";
  bool bPopup = true;

  if(QUriDrag::decode(event, strlist)) {
    if (strlist.count()) {
      for(const char* p = strlist.first(); p; p = strlist.next()) {
        if(file_count++ > 0) {
          dropText += " ";
          bPopup = false; // more than one file, don't popup
        }
        KURL url(QUriDrag::uriToUnicodeUri(p));
        QString tmp;
        if (url.isLocalFile()) {
          tmp = url.path(); // local URL : remove protocol
        }
        else {
          tmp = url.url();
          bPopup = false; // a non-local file, don't popup
        }
        if (strlist.count()>1)
          KRun::shellQuote(tmp);
        dropText += tmp;
      }

      if (bPopup)
        // m_drop->popup(pos() + event->pos());
         m_drop->popup(mapToGlobal(event->pos()));
      else
	{
	  if (currentSession) {
	    currentSession->getEmulation()->sendString(dropText.local8Bit());
	  }
	  kdDebug(1211) << "Drop:" << dropText.local8Bit() << "\n";
	}
    }
  }
  else if(QTextDrag::decode(event, dropText)) {
    kdDebug(1211) << "Drop:" << dropText.local8Bit() << "\n";
    if (currentSession) {
      currentSession->getEmulation()->sendString(dropText.local8Bit());
    }
    // Paste it
  }
}

void TEWidget::doDrag()
{
  dragInfo.state = diDragging;
  dragInfo.dragObject = new QTextDrag(QApplication::clipboard()->text(), this);
  dragInfo.dragObject->dragCopy();
  // Don't delete the QTextDrag object.  Qt will delete it when it's done with it.
}

void TEWidget::drop_menu_activated(int item)
{
   switch (item)
   {
   case 0: // paste
      KRun::shellQuote(dropText);
      currentSession->getEmulation()->sendString(dropText.local8Bit());
      setActiveWindow();
      break;
   case 1: // cd ...
      currentSession->getEmulation()->sendString("cd ");
      struct stat statbuf;
      if ( ::stat( QFile::encodeName( dropText ), &statbuf ) == 0 )
      {
         if ( !S_ISDIR(statbuf.st_mode) )
         {
            KURL url;
            url.setPath( dropText );
            dropText = url.directory( true, false ); // remove filename
         }
      }
      KRun::shellQuote(dropText);
      currentSession->getEmulation()->sendString(dropText.local8Bit());
      currentSession->getEmulation()->sendString("\n");
      setActiveWindow();
      break;
   }
}

uint TEWidget::lineSpacing() const
{
  return m_lineSpacing;
}

void TEWidget::setLineSpacing(uint i)
{
  m_lineSpacing = i;
  setVTFont(font()); // Trigger an update.
}

