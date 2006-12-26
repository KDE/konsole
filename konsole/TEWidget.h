/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

#ifndef TE_WIDGET_H
#define TE_WIDGET_H

#include <qbitarray.h>
#include <qwidget.h>
#include <qcolor.h>
#include <qkeycode.h>
#include <qtimer.h>
#include <qscrollbar.h>

#include <kpopupmenu.h>

#include "TECommon.h"


extern unsigned short vt100_graphics[32];

class Konsole;
class QLabel;
class QTimer;

class TEWidget : public QFrame
// a widget representing attributed text
{
   Q_OBJECT

  friend class Konsole;
public:

    TEWidget(QWidget *parent=0, const char *name=0);
    virtual ~TEWidget();

    void setBlendColor(const QRgb color) { blend_color = color; }

    void setDefaultBackColor(const QColor& color);
    QColor getDefaultBackColor();

    const ColorEntry* getColorTable() const;
    void              setColorTable(const ColorEntry table[]);

    void setScrollbarLocation(int loc);
    int  getScrollbarLocation() { return scrollLoc; }
    enum { SCRNONE=0, SCRLEFT=1, SCRRIGHT=2 };

    void setScroll(int cursor, int lines);
    void doScroll(int lines);

    bool blinkingCursor() { return hasBlinkingCursor; }
    void setBlinkingCursor(bool blink);

    void setCtrlDrag(bool enable) { ctrldrag=enable; }
    bool ctrlDrag() { return ctrldrag; }

    void setCutToBeginningOfLine(bool enable) { cuttobeginningofline=enable; }
    bool cutToBeginningOfLine() { return cuttobeginningofline; }

    void setLineSpacing(uint);
    uint lineSpacing() const;

    void emitSelection(bool useXselection,bool appendReturn);
    void emitText(QString text);

    void setImage(const ca* const newimg, int lines, int columns);
    void setLineWrapped(QBitArray line_wrapped) { m_line_wrapped=line_wrapped; }

    void setCursorPos(const int curx, const int cury);

    int  Lines()   { return lines;   }
    int  Columns() { return columns; }

    int  fontHeight()   { return font_h;   }
    int  fontWidth()    { return font_w; }
    
    void calcGeometry();
    void propagateSize();
    void updateImageSize();
    void setSize(int cols, int lins);
    void setFixedSize(int cols, int lins);
    QSize sizeHint() const;

    void setWordCharacters(QString wc);
    QString wordCharacters() { return word_characters; }

    void setBellMode(int mode);
    int bellMode() { return m_bellMode; }
    enum { BELLSYSTEM=0, BELLNOTIFY=1, BELLVISUAL=2, BELLNONE=3 };
    void Bell(bool visibleSession, QString message);

    void setSelection(const QString &t);

    /** 
     * Reimplemented.  Has no effect.  Use setVTFont() to change the font
     * used to draw characters in the display.
     */
    virtual void setFont(const QFont &);

    /** Returns the font used to draw characters in the display */
    QFont getVTFont() { return font(); }

    /** 
     * Sets the font used to draw the display.  Has no effect if @p font
     * is larger than the size of the display itself.    
     */
    void setVTFont(const QFont& font);

    void setMouseMarks(bool on);
    static void setAntialias( bool enable ) { s_antialias = enable; }
    static bool antialias()                 { return s_antialias;   }
    static void setStandalone( bool standalone ) { s_standalone = standalone; }
    static bool standalone()                { return s_standalone;   }

    void setTerminalSizeHint(bool on) { terminalSizeHint=on; }
    bool isTerminalSizeHint() { return terminalSizeHint; }
    void setTerminalSizeStartup(bool on) { terminalSizeStartup=on; }

    void setBidiEnabled(bool set) { bidiEnabled=set; }
    bool isBidiEnabled() { return bidiEnabled; }
    
    void print(QPainter &paint, bool friendly, bool exact);

    void setRim(int rim) { rimX=rim; rimY=rim; }

public slots:

    void setSelectionEnd();
    void copyClipboard();
    void pasteClipboard();
    void pasteSelection();
    void onClearSelection();

signals:

    void keyPressedSignal(QKeyEvent *e);
    void mouseSignal(int cb, int cx, int cy);
    void changedFontMetricSignal(int height, int width);
    void changedContentSizeSignal(int height, int width);
    void changedHistoryCursor(int value);
    void configureRequest( TEWidget*, int state, int x, int y );

    void copySelectionSignal();
    void clearSelectionSignal();
    void beginSelectionSignal( const int x, const int y, const bool columnmode );
    void extendSelectionSignal( const int x, const int y );
    void endSelectionSignal(const bool preserve_line_breaks);
    void isBusySelecting(bool);
    void testIsSelected(const int x, const int y, bool &selected /* result */);
  void sendStringToEmu(const char*);

protected:

    virtual void styleChange( QStyle& );

    bool eventFilter( QObject *, QEvent * );
    bool event( QEvent * );

    void drawTextFixed(QPainter &paint, int x, int y,
                       QString& str, const ca *attr);

    void drawAttrStr(QPainter &paint, QRect rect,
                     QString& str, const ca *attr, bool pm, bool clear);
    void paintEvent( QPaintEvent * );

    void paintContents(QPainter &paint, const QRect &rect, bool pm=false);

    void resizeEvent(QResizeEvent*);

    void fontChange(const QFont &font);
    void frameChanged();

    void mouseDoubleClickEvent(QMouseEvent* ev);
    void mousePressEvent( QMouseEvent* );
    void mouseReleaseEvent( QMouseEvent* );
    void mouseMoveEvent( QMouseEvent* );
    void extendSelection( QPoint pos );
    void wheelEvent( QWheelEvent* );

    void focusInEvent( QFocusEvent * );
    void focusOutEvent( QFocusEvent * );
    bool focusNextPrevChild( bool next );
    // Dnd
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);
    void doDrag();
    enum DragState { diNone, diPending, diDragging };

    struct _dragInfo {
      DragState       state;
      QPoint          start;
      QTextDrag       *dragObject;
    } dragInfo;

    virtual int charClass(UINT16) const;

    void clearImage();

    void mouseTripleClickEvent(QMouseEvent* ev);

    void imStartEvent( QIMEvent *e );
    void imComposeEvent( QIMEvent *e );
    void imEndEvent( QIMEvent *e );

protected slots:

    void scrollChanged(int value);
    void blinkEvent();
    void blinkCursorEvent();

private:

//    QChar (*fontMap)(QChar); // possible vt100 font extension

    bool fixed_font; // has fixed pitch
    int  font_h;     // height
    int  font_w;     // width
    int  font_a;     // ascend

    int bX;    // offset
    int bY;    // offset

    int lines;
    int columns;
    int contentHeight;
    int contentWidth;
    ca *image; // [lines][columns]
    int image_size;
    QBitArray m_line_wrapped;

    ColorEntry color_table[TABLE_COLORS];
    QColor defaultBgColor;

    bool resizing;
    bool terminalSizeHint,terminalSizeStartup;
    bool bidiEnabled;
    bool mouse_marks;

    void makeImage();

    QPoint iPntSel; // initial selection point
    QPoint pntSel; // current selection point
    QPoint tripleSelBegin; // help avoid flicker
    int    actSel; // selection state
    bool    word_selection_mode;
    bool    line_selection_mode;
    bool    preserve_line_breaks;
    bool    column_selection_mode;

    QClipboard*    cb;
    QScrollBar* scrollbar;
    int         scrollLoc;
    QString     word_characters;
    QTimer      bellTimer; //used to rate-limit bell events.  started when a bell event occurs,
                           //and prevents further bell events until it stops
    int         m_bellMode;

    bool blinking;   // hide text in paintEvent
    bool hasBlinker; // has characters to blink
    bool cursorBlinking;     // hide cursor in paintEvent
    bool hasBlinkingCursor;  // has blinking cursor enabled
    bool ctrldrag;           // require Ctrl key for drag
    bool cuttobeginningofline; // triple click only selects forward
    bool isBlinkEvent; // paintEvent due to blinking.
    bool isPrinting; // Paint job is intended for printer
    bool printerFriendly; // paint printer friendly, save ink
    bool printerBold; // Use a bold font instead of overstrike for bold
    bool isFixedSize; //Columns / lines are locked.
    QTimer* blinkT;  // active when hasBlinker
    QTimer* blinkCursorT;  // active when hasBlinkingCursor

    KPopupMenu* m_drop;
    QString dropText;
    int m_dnd_file_count;

    bool possibleTripleClick;  // is set in mouseDoubleClickEvent and deleted
                               // after QApplication::doubleClickInterval() delay

    static bool s_antialias;   // do we antialias or not
    static bool s_standalone;  // are we part of a standalone konsole?

    QFrame *mResizeWidget;
    QLabel *mResizeLabel;
    QTimer *mResizeTimer;

    uint m_lineSpacing;

    QRect       cursorRect; //for quick changing of cursor

    QPoint configureRequestPoint;  // remember right mouse button click position
    bool colorsSwapped; // true during visual bell

    // the rim should normally be 1, 0 only when running in full screen mode.
    int rimX;      // left/right rim width
    int rimY;      // top/bottom rim high
    QSize m_size;

    QString m_imPreeditText;
    int m_imPreeditLength;
    int m_imStart;
    int m_imStartLine;
    int m_imEnd;
    int m_imSelStart;
    int m_imSelEnd;
    int m_cursorLine;
    int m_cursorCol;
    bool m_isIMEdit;
    bool m_isIMSel;

    QRgb blend_color;
 
private slots:
    void drop_menu_activated(int item);
    void swapColorTable();
    void tripleClickTimeout();  // resets possibleTripleClick
};

#endif // TE_WIDGET_H
