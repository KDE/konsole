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

#include <QBitArray>
#include <QWidget>
#include <QColor>
#include <qnamespace.h>
#include <qscrollbar.h>
#include <QFrame>
#include <QInputMethodEvent>
//Added by qt3to4:
#include <QWheelEvent>
#include <QFocusEvent>
#include <QPaintEvent>
#include <QKeyEvent>
#include <QEvent>
#include <QDropEvent>
#include <QLabel>
#include <QResizeEvent>
#include <QDragEnterEvent>
#include <QMouseEvent>
//Should have been added by qt3to4:
// #include <Q3TextDrag>
#include <QDrag>

#include <kmenu.h>

#include "TECommon.h"


extern unsigned short vt100_graphics[32];

class Konsole;
class QLabel;
class QTimer;
class QFrame;
class QGridLayout;

class TEWidget : public QFrame
// a widget representing attributed text
{
   Q_OBJECT

  friend class Konsole;
public:

    TEWidget(QWidget *parent=0);
    virtual ~TEWidget();

    void setBlendColor(const QRgb color) { blend_color = color; }

    void setDefaultBackColor(const QColor& color);
    QColor getDefaultBackColor();

    const ColorEntry* getColorTable() const;
    void              setColorTable(const ColorEntry table[]);

    void setScrollbarLocation(int loc);
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
    void setLineProperties(QVector<LineProperty> properties) { lineProperties=properties; }

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

    virtual void setFont(const QFont &);
    QFont getVTFont() { return font(); }
    void setVTFont(const QFont &);

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

public Q_SLOTS:

    void setSelectionEnd();
    void copyClipboard();
    void pasteClipboard();
    void pasteSelection();
    void onClearSelection();
	/** 
	 * Causes the widget to display or hide a message informing the user that terminal
	 * output has been suspended (by using the flow control key combination Ctrl+S)
	 *
	 * @param suspended True if terminal output has been suspended and the warning message should
	 *				 	be shown or false to indicate that terminal output has been resumed and that
	 *				 	the warning message should disappear.
	 */ 
	void outputSuspended(bool suspended);

Q_SIGNALS:

    void keyPressedSignal(QKeyEvent *e);

    /**
     * Emitted when the user presses the suspend or resume flow control key combinations 
     * 
     * @param suspend true if the user pressed Ctrl+S (the suspend output key combination) or
     * false if the user pressed Ctrl+Q (the resume output key combination)
     */
    void flowControlKeyPressed(bool suspend);
    
    /** 
     * A mouse event occurred.
     * @param cb The mouse button (0 for left button, 1 for middle button, 2 for right button, 3 for release)
     * @param cx The character column where the event occurred
     * @param cy The character row where the event occurred
     * @param eventType The type of event.  0 for a mouse press / release or 1 for mouse motion
     */
    void mouseSignal(int cb, int cx, int cy, int eventType);
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

	//draws a string of normal text characters
	//The painter's font and other attributes needed to draw the string
	//should be set before drawTextFixed is called.
    void drawTextFixed(QPainter &paint, int x, int y,
                       QString& str, const ca *attr);

	//draws a string of line-drawing characters
	void drawLineCharString(QPainter& painter, int x, int y, const QString& str, const ca* attributes);

    void drawAttrStr(QPainter &paint, const QRect& rect,
                     QString& str, const ca *attr, bool pm, bool clear);
    void paintEvent( QPaintEvent * );

    void paintContents(QPainter &paint, const QRect &rect);

    void resizeEvent(QResizeEvent*);

    void fontChange(const QFont &font);
    void frameChanged();

    void mouseDoubleClickEvent(QMouseEvent* ev);
    void mousePressEvent( QMouseEvent* );
    void mouseReleaseEvent( QMouseEvent* );
    void mouseMoveEvent( QMouseEvent* );
    void extendSelection( QPoint pos );
    void wheelEvent( QWheelEvent* );

    bool focusNextPrevChild( bool next );
    // Dnd
    void dragEnterEvent(QDragEnterEvent* event);
    void dropEvent(QDropEvent* event);
    void doDrag();
    enum DragState { diNone, diPending, diDragging };

    struct _dragInfo {
      DragState       state;
      QPoint          start;
      QDrag           *dragObject;
    } dragInfo;

    virtual int charClass(UINT16) const;

    void clearImage();

    void mouseTripleClickEvent(QMouseEvent* ev);

    void inputMethodEvent ( QInputMethodEvent * e );

protected Q_SLOTS:

    void scrollChanged(int value);
    void blinkEvent();
    void blinkCursorEvent();

private:

//    QChar (*fontMap)(QChar); // possible vt100 font extension

    QGridLayout* gridLayout;

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
    QVector<LineProperty> lineProperties;

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
    int         m_bellMode;

    bool blinking;   // hide text in paintEvent
    bool hasBlinker; // has characters to blink
    bool cursorBlinking;     // hide cursor in paintEvent
    bool hasBlinkingCursor;  // has blinking cursor enabled
    bool ctrldrag;           // require Ctrl key for drag
    bool cuttobeginningofline; // triple click only selects forward
    bool isPrinting; // Paint job is intended for printer
    bool printerFriendly; // paint printer friendly, save ink
    bool printerBold; // Use a bold font instead of overstrike for bold
    bool isFixedSize; //Columns / lines are locked.
    QTimer* blinkT;  // active when hasBlinker
    QTimer* blinkCursorT;  // active when hasBlinkingCursor

    KMenu* m_drop;
    QString dropText;
    int m_dnd_file_count;

    bool possibleTripleClick;  // is set in mouseDoubleClickEvent and deleted
                               // after QApplication::doubleClickInterval() delay

    static bool s_antialias;   // do we antialias or not
    static bool s_standalone;  // are we part of a standalone konsole?

    QFrame *mResizeWidget;
    QLabel *mResizeLabel;
    QTimer *mResizeTimer;

	//widgets related to the warning message that appears when the user presses Ctrl+S to suspend
	//terminal output - informing them what has happened and how to resume output
	QLabel* outputSuspendedLabel; 
    	
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

    QAction* m_pasteAction;
    QAction* m_cdAction;
    QAction* m_cpAction;
    QAction* m_mvAction;
    QAction* m_lnAction;

	//the delay in milliseconds between redrawing blinking text
	static const int BLINK_DELAY = 750;

private Q_SLOTS:
    void drop_menu_activated(QAction*);
    void swapColorTable();
    void tripleClickTimeout();  // resets possibleTripleClick
};

#endif // TE_WIDGET_H
