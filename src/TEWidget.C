/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [TEWidget.C]            Terminal Emulation Widget                          */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

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

#include <qpainter.h>
#include <qkeycode.h>
#include <qclipbrd.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>

#include <assert.h>

#include "TEWidget.moc"
#include <kapp.h>
#include <kmsgbox.h>
#include <X11/Xlib.h>

#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)
#define HCNT(Name) // { static int cnt = 1; printf("%s(%d): %s %d\n",__FILE__,__LINE__,Name,cnt++); }

#define loc(X,Y) ((Y)*columns+(X))

#define rimX 1      // left/right rim width
#define rimY 1      // top/bottom rim high

#define SCRWIDTH 16 // width of the scrollbar

#define yMouseScroll 1
// scroll increment used when dragging selection at top/bottom of window.

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Colors                                     */
/*                                                                           */
/* ------------------------------------------------------------------------- */

static const ColorEntry base_color_table[TABLE_COLORS] =
// The following are almost IBM standard color codes, with some slight
// gamma correction for the dim colors to compensate for bright X screens.
// It contains the 8 ansiterm/xterm colors in 2 intensities.
{
  // Fixme: could add faint colors here, also.
  // normal
  { QColor(0x00,0x00,0x00), 0, 0 }, { QColor(0xB2,0xB2,0xB2), 1, 0 }, // Dfore, Dback
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

void TEWidget::setVTFont(const QFont& f)
{
  QFrame::setFont(f);
}

void TEWidget::setFont(const QFont &)
{
  // ignore font change request if not coming from konsole itself
}

//FIXME: add backgroundPixmapChanged.

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Constructor / Destructor                          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

TEWidget::TEWidget(QWidget *parent, const char *name) : QFrame(parent,name)
{
  cb = QApplication::clipboard();   
  QObject::connect( (QObject*)cb, SIGNAL(dataChanged()), 
                    this, SLOT(onClearSelection()) );

  scrollbar = new QScrollBar(this);
  scrollbar->setCursor( arrowCursor );
  connect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(scrollChanged(int)));
  scrollLoc = SCRNONE;

  blinkT   = new QTimer(this);
  connect(blinkT, SIGNAL(timeout()), this, SLOT(blinkEvent()));
  blinking = FALSE;

  resizing = FALSE;
  actSel   = 0;
  image    = 0;
  lines    = 1;
  columns  = 1;
  font_w   = 1;
  font_h   = 1;
  font_a   = 1;
  word_selection_mode = FALSE;

  setMouseMarks(TRUE);
  setVTFont( QFont("fixed") );
  
  setColorTable(base_color_table); // init color table

  if ( parent ) parent->installEventFilter( this ); //FIXME: see below
}

//FIXME: make proper destructor

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Display Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*!
    attributed string draw primitive
*/

void TEWidget::drawAttrStr(QPainter &paint, QRect rect,
                           char* str, int len, ca attr, BOOL pm)
{
  if (pm && color_table[attr.b].transparent)
  {
    paint.setBackgroundMode( TransparentMode );
    erase(rect);
  }
  else
  {
    if (blinking)
      paint.fillRect(rect, color_table[attr.b].color);
    else
    {
      paint.setBackgroundMode( OpaqueMode );
      paint.setBackgroundColor( color_table[attr.b].color );
    }
  }
  if (!(blinking && (attr.r & RE_BLINK)))
  {
    paint.setPen(color_table[attr.f].color); 
    paint.drawText(rect.x(),rect.y()+font_a, str,len);
    if ((attr.r & RE_UNDERLINE) || color_table[attr.f].bold)
    {
      paint.setClipRect(rect);
      if (color_table[attr.f].bold)
      {
        paint.setBackgroundMode( TransparentMode );
        paint.drawText(rect.x()+1,rect.y()+font_a, str,len);
      }
      if (attr.r & RE_UNDERLINE)
        paint.drawLine(rect.left(), rect.y()+font_a+1,
                       rect.right(),rect.y()+font_a+1 );
      paint.setClipping(FALSE);
    }
  }
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
  
  int lins = MIN(this->lines,  MAX(0,lines  ));
  int cols = MIN(this->columns,MAX(0,columns));
  char *disstr = new char[cols];

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
        cr = ext[x].r;
        cb = ext[x].b; 
        if (ext[x].f != cf) cf = ext[x].f;
        int lln = cols - x;
        disstr[0] = ext[x+0].c;
        for (len = 1; len < lln; len++)
        {
          if (ext[x+len].f != cf || ext[x+len].b != cb || ext[x+len].r != cr ||
              ext[x+len] == lcl[x+len] )
            break;
          disstr[len] = ext[x+len].c;
        }
        drawAttrStr(paint, 
                    QRect(blX+tLx+font_w*x,bY+tLy+font_h*y,font_w*len,font_h),
                    disstr, len, ext[x], pm != NULL);
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
  if (!hasBlinker &&  blinkT->isActive()) { blinkT->stop(); blinking = FALSE; }
  delete disstr;
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

  // FIXME: explain/change 'rounding' style

  QRect rect = pe->rect().intersect(contentsRect());
  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();
 
  int lux = MIN(columns-1, MAX(0,(rect.left()   - tLx - blX ) / font_w));
  int luy = MIN(lines-1,   MAX(0,(rect.top()    - tLy - bY  ) / font_h));
  int rlx = MIN(columns-1, MAX(0,(rect.right()  - tLx - blX ) / font_w));
  int rly = MIN(lines-1,   MAX(0,(rect.bottom() - tLy - bY  ) / font_h));

/*
 printf("paintEvent: %d..%d, %d..%d (%d..%d, %d..%d)\n",lux,rlx,luy,rly,
  rect.left(), rect.right(), rect.top(), rect.bottom());
*/

  for (int y = luy; y <= rly; y++)
  for (int x = lux; x <= rlx; x++)
  { char *disstr = new char[columns]; int len = 1;
    disstr[0] = image[loc(x,y)].c; 
    int cf = image[loc(x,y)].f;
    int cb = image[loc(x,y)].b;
    int cr = image[loc(x,y)].r;
    while (x+len <= rlx && 
           image[loc(x+len,y)].f == cf &&
           image[loc(x+len,y)].b == cb &&
           image[loc(x+len,y)].r == cr )
    {
      disstr[len] = image[loc(x+len,y)].c; len += 1;
    }
    drawAttrStr(paint, 
                QRect(blX+tLx+font_w*x,bY+tLy+font_h*y,font_w*len,font_h),
                disstr, len, image[loc(x,y)], pm != NULL);
    x += len - 1;
    delete disstr;
  }
  drawFrame( &paint );
  paint.end();
  setUpdatesEnabled(TRUE);
}

void TEWidget::blinkEvent()
{
  blinking = !blinking;
  repaint(FALSE);
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
  int lins = MIN(oldlin,lines);
  int cols = MIN(oldcol,columns);
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
  disconnect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(scrollChanged(int)));
  scrollbar->setRange(0,slines);
  scrollbar->setSteps(1,lines);
  scrollbar->setValue(cursor);
  connect(scrollbar, SIGNAL(valueChanged(int)), this, SLOT(scrollChanged(int)));
}

void TEWidget::setScrollbarLocation(int loc)
{
  if (scrollLoc == loc) return; // quickly
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
    routines in this section serve both.
    1) The press/release events are exposed (to the application)
    2) Marking (press and move left button)  and Pasting (press middle button)
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
// printf("press [%d,%d] %d\n",ev->x()/font_w,ev->y()/font_h,ev->button());
  if ( !contentsRect().contains(ev->pos()) ) return;
  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();

  word_selection_mode = FALSE;

// printf("press top left [%d,%d] by=%d\n",tLx,tLy, bY);
  if ( ev->button() == LeftButton)
  {
    QPoint pos = QPoint((ev->x()-tLx-blX)/font_w,(ev->y()-tLy-bY)/font_h);

    if ( ev->state() & ControlButton ) preserve_line_breaks = FALSE ;

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
  if ( ev->button() == MidButton )
  {
    emitSelection();
  }
  if ( ev->button() == RightButton ) // Configure
  {
    emit configureRequest( this, ev->state()&(ShiftButton|ControlButton), ev->x(), ev->y() );
  }
}

void TEWidget::mouseMoveEvent(QMouseEvent* ev)
{
  if (actSel == 0) return;
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
  if ( pos.x() < tLx+blX )                  pos.setX( tLx+blX );
  if ( pos.x() > tLx+blX+columns*font_w-1 ) pos.setX( tLx+blX+columns*font_w );
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

  QPoint here = QPoint((pos.x()-tLx-blX)/font_w,(pos.y()-tLy-bY)/font_h);
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
    while ( left.x() > 0 && charClass(image[i-1].c) == selClass )
    { i--; left.rx()--; }

    // Find left (left_not_right ? from start : from here)
    QPoint right = left_not_right ? iPntSel : here;
    i = loc(right.x(),right.y());
    selClass = charClass(image[i].c);
    while ( right.x() < columns-1 && charClass(image[i+1].c) == selClass )
    { i++; right.rx()++; }

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

  if ( word_selection_mode ) {
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
//  printf("release [%d,%d] %d\n",ev->x()/font_w,ev->y()/font_h,ev->button());
  if ( ev->button() == LeftButton)
  {
    if ( actSel > 1 ) emit endSelectionSignal(preserve_line_breaks);
    preserve_line_breaks = TRUE;

    //FIXME: emits a release event even if the mouse is
    //       outside the range. The procedure used in `mouseMoveEvent'
    //       applies here, too.

    QPoint tL  = contentsRect().topLeft();
    int    tLx = tL.x();
    int    tLy = tL.y();

    if (!mouse_marks && !(ev->state() & ShiftButton))
      emit mouseSignal( 3, // release
                        (ev->x()-tLx-blX)/font_w + 1,
                        (ev->y()-tLy-bY)/font_h + 1 );
    releaseMouse();
  }
}

void TEWidget::mouseDoubleClickEvent(QMouseEvent* ev)
{
  if ( ev->button() != LeftButton) return;

  QPoint tL  = contentsRect().topLeft();
  int    tLx = tL.x();
  int    tLy = tL.y();
  QPoint pos = QPoint((ev->x()-tLx-blX)/font_w,(ev->y()-tLy-bY)/font_h);

  // pass on double click as two clicks.
  if (!mouse_marks && !(ev->state() & ShiftButton))
  {
    emit mouseSignal( 0, pos.x()+1, pos.y()+1 ); // left button
    emit mouseSignal( 3, pos.x()+1, pos.y()+1 ); // release
    emit mouseSignal( 0, pos.x()+1, pos.y()+1 ); // left button
    return;
  }
     
  
  emit clearSelectionSignal();
  QPoint bgnSel = pos;
  QPoint endSel = QPoint((ev->x()-tLx-blX)/font_w,(ev->y()-tLy-bY)/font_h);
  int i = loc(bgnSel.x(),bgnSel.y());
  iPntSel = bgnSel;

  word_selection_mode = TRUE;

  // find word boundaries...
  int selClass = charClass(image[i].c);
  {
    // set the start...
     int x = bgnSel.x();
     while ( x > 0 && charClass(image[i-1].c) == selClass )
     { i--; x--; }
     bgnSel.setX(x);
     emit beginSelectionSignal( bgnSel.x(), bgnSel.y() );

     // set the end...
     i = loc( endSel.x(), endSel.y() );
     x = endSel.x();
     while( x < columns-1 && charClass(image[i+1].c) == selClass )
     { i++; x++ ; }
     endSel.setX(x);
     emit extendSelectionSignal( endSel.x(), endSel.y() );
     emit endSelectionSignal(preserve_line_breaks);
     preserve_line_breaks = TRUE;
   }
}

int TEWidget::charClass(char ch) const
{
    // This might seem like overkill, but imagine if ch was a Unicode
    // character (Qt 2.0 QChar) - it might then be sensible to separate
    // the different language ranges, etc.

    if ( isspace(ch) ) return ' ';

    static char *word_characters = "@-./_~";
    if ( isalnum(ch) || strchr(word_characters, ch) )
    return 'a';

    // Everything else is weird
    return 1;
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

void TEWidget::emitSelection()
// Paste Clipboard by simulating keypress events
{ const char *text = QApplication::clipboard()->text();
  if ( text ) while (*text)
  { QKeyEvent e(0,0,(int)(unsigned char)*text++,0);
    emit keyPressedSignal(&e); // expose as keypress event
  }
}

void TEWidget::setSelection(const char *t)
{
  // Disconnect signal while WE set the clipboard
  QObject *cb = QApplication::clipboard();   
  QObject::disconnect( cb, SIGNAL(dataChanged()), 
                     this, SLOT(onClearSelection()) );

  QApplication::clipboard()->setText(t);

  QObject::connect( cb, SIGNAL(dataChanged()), 
                     this, SLOT(onClearSelection()) );
  return;
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
//       repaintevents being emitted to the screen whenever one leaves
//       or reenters the screen to/from another application.
//
//   Troll says you need to change focusInEvent() and focusOutEvent(),
//   which would also let you have an in-focus cursor and an out-focus
//   cursor like xterm does.

bool TEWidget::eventFilter( QObject *, QEvent *e )
{
  if ( e->type() == Event_KeyPress )
  { QKeyEvent* ke = (QKeyEvent*)e;

    actSel=0; // Key stroke implies a screen update, so TEWidget won't
              // know where the current selection is.
 
    switch (ke->state() | (ke->key() << 8))
    {
      case ShiftButton|(Key_PageUp   << 8) : 
           scrollbar->setValue(scrollbar->value()-lines/2);
           break;
      case ShiftButton|(Key_PageDown << 8) :
           scrollbar->setValue(scrollbar->value()+lines/2);
           break;
      case ShiftButton|(Key_Up       << 8) : 
           scrollbar->setValue(scrollbar->value()-1);
           break;
      case ShiftButton|(Key_Down     << 8) :
           scrollbar->setValue(scrollbar->value()+1);
           break;
      case ShiftButton|(Key_Insert   << 8) :
           emitSelection();
           break;
      default :
           emit keyPressedSignal(ke); // expose
           break;
    }
    return TRUE; // accept event
  } 
  else
  {
    if ( e->type() == Event_Enter )
    {
        QObject::disconnect( (QObject*)cb, SIGNAL(dataChanged()), 
        this, SLOT(onClearSelection()) );
    }
    if ( e->type() == Event_Leave )
    {
        QObject::connect( (QObject*)cb, SIGNAL(dataChanged()), 
        this, SLOT(onClearSelection()) );
    }
  }
  return FALSE; // standard event processing
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                   Font                                    */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TEWidget::fontChange(const QFont &)
{
  font_h = fontMetrics().height();
  font_w = fontMetrics().maxWidth();
  font_a = fontMetrics().ascent();
  propagateSize();
  update();
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

void TEWidget::Bell()
{
  XBell(qt_xdisplay(),0);
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
  scrollbar->resize(SCRWIDTH, contentsRect().height());
  switch(scrollLoc)
  {
    case SCRNONE : 
     columns = ( contentsRect().width() - 2 * rimX ) / font_w;
     blX = (contentsRect().width() - (columns*font_w) ) / 2;
     brX = blX;
     scrollbar->hide();
     break;
    case SCRLEFT : 
     columns = ( contentsRect().width() - 2 * rimX - SCRWIDTH) / font_w;
     brX = (contentsRect().width() - (columns*font_w) - SCRWIDTH ) / 2;
     blX = brX + SCRWIDTH;
     scrollbar->move(contentsRect().topLeft());
     scrollbar->show();
     break;
    case SCRRIGHT:
     columns = ( contentsRect().width()  - 2 * rimX - SCRWIDTH) / font_w;
     blX = (contentsRect().width() - (columns*font_w) - SCRWIDTH ) / 2;
     brX = blX;
     scrollbar->move(contentsRect().topRight() - QPoint(SCRWIDTH-1,0));
     scrollbar->show();
     break;
  }
  //FIXME: support 'rounding' styles
  lines   = ( contentsRect().height() - 2 * rimY  ) / font_h;
  bY = (contentsRect().height() - (lines  *font_h)) / 2;
}

void TEWidget::makeImage()
//FIXME: rename 'calcGeometry?
{
  calcGeometry();
  image = (ca*) malloc(lines*columns*sizeof(ca));
  clearImage();
}

// calculate the needed size
QSize TEWidget::calcSize(int cols, int lins)
{ int frw = width() - contentsRect().width();
  int frh = height() - contentsRect().height();
  int scw = (scrollLoc==SCRNONE?0:SCRWIDTH);
  return QSize( font_w*cols + 2*rimX + frw + scw, font_h*lins + 2*rimY + frh );
}
