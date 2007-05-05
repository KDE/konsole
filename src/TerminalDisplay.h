/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>
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

#ifndef TERMINALDISPLAY_H
#define TERMINALDISPLAY_H

// Qt
#include <QBitArray>
#include <QWidget>
#include <QColor>
#include <qnamespace.h>
#include <qscrollbar.h>
#include <QFrame>
#include <QInputMethodEvent>
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
#include <QDrag>

// KDE
#include <kmenu.h>

// Konsole
#include "TECommon.h"

class QLabel;
class QTimer;
class QFrame;
class QGridLayout;
class QShowEvent;
class QHideEvent;

namespace Konsole
{

extern unsigned short vt100_graphics[32];

class TerminalImageFilterChain;
namespace Filter
{
    class HotSpot;
};
class FilterChain;
class ScreenWindow;

/**
 * A widget which displays output from a terminal emulation and sends input keypresses and mouse activity
 * to the terminal.
 *
 * When the terminal emulation receives new output from the program running in the terminal, 
 * it will update the display by calling updateImage().
 *
 * TODO More documentation
 */
class TerminalDisplay : public QFrame
{
   Q_OBJECT

public:
    /** Constructs a new terminal display widget with the specified parent. */
    TerminalDisplay(QWidget *parent=0);
    virtual ~TerminalDisplay();

    void setBlendColor(const QRgb color) { _blendColor = color; }

    /** Sets the default background color for the display. */
    void setDefaultBackColor(const QColor& color);
    /** Returns the default background color for the display */
    QColor defaultBackColor();

    /** Returns the terminal color palette used by the display. */
    const ColorEntry* colorTable() const;
    /** Sets the terminal color palette used by the display. */
    void setColorTable(const ColorEntry table[]);

    /** 
     * This enum describes the location where the scroll bar is positioned in the display widget.
     */
    enum ScrollBarLocation 
    { 
        /** Do not show the scroll bar. */
        SCROLLBAR_NONE=0, 
        /** Show the scroll bar on the left side of the display. */
        SCROLLBAR_LEFT=1, 
        /** Show the scroll bar on the right side of the display. */
        SCROLLBAR_RIGHT=2 
    };
    /** 
     * Specifies whether the terminal display has a vertical scroll bar, and if so whether it
     * is shown on the left or right side of the display.
     */
    void setScrollBarLocation(ScrollBarLocation loc);

    /** 
     * Sets the current position and range of the display's scroll bar.
     *
     * @param cursor The position of the scroll bar's thumb.
     * @param lines The maximum value of the scroll bar.
     */
    void setScroll(int cursor, int lines);

    /** 
     * Returns the display's filter chain.  When the image for the display is updated,
     * the text is passed through each filter in the chain.  Each filter can define
     * hotspots which correspond to certain strings (such as URLs or particular words).
     * Depending on the type of the hotspots created by the filter ( returned by Filter::Hotspot::type() )
     * the view will draw visual cues such as underlines on mouse-over for links or translucent
     * rectangles for markers.
     *
     * To add a new filter to the view, call:
     *      viewWidget->filterChain()->addFilter( filterObject );
     */
    FilterChain* filterChain() const;

    /** 
     * Updates the filters in the display's filter chain.  This will cause
     * the hotspots to be updated to match the current image.
     *
     * WARNING:  This function can be expensive depending on the 
     * image size and number of filters in the filterChain()
     *
     * TODO - This API does not really allow efficient usage.  Revise it so
     * that the processing can be done in a better way.
     *
     * eg:
     *      - Area of interest may be known ( eg. mouse cursor hovering
     *      over an area )
     */  
    void processFilters();

    /** Returns true if the cursor is set to blink or false otherwise. */
    bool blinkingCursor() { return _hasBlinkingCursor; }
    /** Specifies whether or not the cursor blinks. */
    void setBlinkingCursor(bool blink);

    void setCtrlDrag(bool enable) { _ctrlDrag=enable; }
    bool ctrlDrag() { return _ctrlDrag; }

    void setCutToBeginningOfLine(bool enable) { _cutToBeginningOfLine=enable; }
    bool cutToBeginningOfLine() { return _cutToBeginningOfLine; }

    void setLineSpacing(uint);
    uint lineSpacing() const;

    void emitSelection(bool useXselection,bool appendReturn);

    void setCursorPos(const int curx, const int cury);

    /**
     * This enum describes the available shapes for the keyboard cursor.
     * See setKeyboardCursorShape()
     */
    enum KeyboardCursorShape
    {
        /** A rectangular block which covers the entire area of the cursor character. */
        BlockCursor,
        /** 
         * A single flat line which occupies the space at the bottom of the cursor
         * character's area.
         */
        UnderlineCursor,
        /** 
         * An cursor shaped like the capital letter 'I', similar to the IBeam 
         * cursor used in Qt/KDE text editors.
         */
        IBeamCursor
    };
    /** 
     * Sets the shape of the keyboard cursor.  This is the cursor drawn   
     * at the position in the terminal where keyboard input will appear.
     *
     * In addition the terminal display widget also has a cursor for 
     * the mouse pointer, which can be set using the QWidget::setCursor()
     * method.
     *
     * Defaults to BlockCursor
     */
    void setKeyboardCursorShape(KeyboardCursorShape shape);
    /**
     * Returns the shape of the keyboard cursor.  See setKeyboardCursorShape()
     */
    KeyboardCursorShape keyboardCursorShape() const;

    /**
     * Sets the color used to draw the keyboard cursor.  
     *
     * The keyboard cursor defaults to using the foreground color of the character
     * underneath it.
     *
     * @param useForegroundColor If true, the cursor color will change to match
     * the foreground color of the character underneath it as it is moved, in this
     * case, the @p color parameter is ignored and the color of the character
     * under the cursor is inverted to ensure that it is still readable.
     * @param color The color to use to draw the cursor.  This is only taken into
     * account if @p useForegroundColor is false.
     */
    void setKeyboardCursorColor(bool useForegroundColor , const QColor& color);

    /** 
     * Returns the color of the keyboard cursor, or an invalid color if the keyboard
     * cursor color is set to change according to the foreground color of the character
     * underneath it. 
     */
    QColor keyboardCursorColor() const;

    /**
     * Returns the number of lines of text which can be displayed in the widget.
     *
     * This will depend upon the height of the widget and the current font.
     * See fontHeight()
     */
    int  lines()   { return _lines;   }
    /**
     * Returns the number of characters of text which can be displayed on
     * each line in the widget.
     *
     * This will depend upon the width of the widget and the current font.
     * See fontWidth()
     */
    int  columns() { return _columns; }

    /**
     * Returns the height of the characters in the font used to draw the text in the display.
     */
    int  fontHeight()   { return _fontHeight;   }
    /**
     * Returns the width of the characters in the display.  
     * This assumes the use of a fixed-width font.
     */
    int  fontWidth()    { return _fontWidth; }

    void setSize(int cols, int lins);
    void setFixedSize(int cols, int lins);
    QSize sizeHint() const;

    /**
     * Sets which characters, in addition to letters and numbers, 
     * are regarded as being part of a word for the purposes
     * of selecting words in the display by double clicking on them.
     *
     * The word boundaries occur at the first and last characters which
     * are either a letter, number, or a character in @p wc
     *
     * @param wc An array of characters which are to be considered parts
     * of a word ( in addition to letters and numbers ).
     */
    void setWordCharacters(QString wc);
    /** 
     * Returns the characters which are considered part of a word for the 
     * purpose of selecting words in the display with the mouse.
     *
     * @see setWordCharacters()
     */
    QString wordCharacters() { return _wordCharacters; }

    /** 
     * Sets the type of effect used to alert the user when a 'bell' occurs in the 
     * terminal session.
     *
     * The terminal session can trigger the bell effect by calling bell() with
     * the alert message.
     */
    void setBellMode(int mode);
    /** 
     * Returns the type of effect used to alert the user when a 'bell' occurs in
     * the terminal session.
     * 
     * See setBellMode()
     */
    int bellMode() { return _bellMode; }

    /**
     * This enum describes the different types of sounds and visual effects which
     * can be used to alert the user when a 'bell' occurs in the terminal
     * session.
     */
    enum BellMode
    { 
        /** A system beep. */
        BELL_SYSTEM=0, 
        /** 
         * KDE notification.  This may play a sound, show a passive popup
         * or perform some other action depending on the user's settings.
         */
        BELL_NOTIFY=1, 
        /** A silent, visual bell (eg. inverting the display's colors briefly) */
        BELL_VISUAL=2, 
        /** No bell effects */
        BELL_NONE=3 
    };

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

    static void setAntialias( bool enable ) { s_antialias = enable; }
    static bool antialias()                 { return s_antialias;   }
    static void setStandalone( bool standalone ) { s_standalone = standalone; }
    static bool standalone()                { return s_standalone;   }

    /**
     * Sets whether or not the current height and width of the 
     * terminal in lines and columns is displayed whilst the widget
     * is being resized.
     */
    void setTerminalSizeHint(bool on) { _terminalSizeHint=on; }
    /** 
     * Returns whether or not the current height and width of
     * the terminal in lines and columns is displayed whilst the widget
     * is being resized.
     */
    bool isTerminalSizeHint() { return _terminalSizeHint; }
    /** 
     * Sets whether the terminal size display is shown briefly
     * after the widget is first shown.
     *
     * See setTerminalSizeHint() , isTerminalSizeHint()
     */
    void setTerminalSizeStartup(bool on) { _terminalSizeStartup=on; }

    void setBidiEnabled(bool set) { _bidiEnabled=set; }
    bool isBidiEnabled() { return _bidiEnabled; }

    void print(QPainter &paint, bool friendly, bool exact);

    void setRim(int rim) { _rimX=rim; _rimY=rim; }

    /**
     * Sets the terminal screen section which is displayed in this widget.
     * When updateImage() is called, the display fetches the latest character image from the
     * the associated terminal screen window.
     *
     * In terms of the model-view paradigm, the ScreenWindow is the model which is rendered
     * by the TerminalDisplay.
     */
    void setScreenWindow( ScreenWindow* window );
    /** Returns the terminal screen section which is displayed in this widget.  See setScreenWindow() */
    ScreenWindow* screenWindow() const;

public Q_SLOTS:

    /** 
     * Causes the terminal display to fetch the latest character image from the associated
     * terminal screen ( see setScreenWindow() ) and redraw the display.
     */
    void updateImage(); 
    /**
     * Causes the terminal display to fetch the latest line status flags from the 
     * associated terminal screen ( see setScreenWindow() ).  
     */ 
    void updateLineProperties();

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

    /**
     * Sets whether the program whoose output is being displayed in the view
     * is interested in mouse events.
     *
     * If this is set to true, mouse signals will be emitted by the view when the user clicks, drags
     * or otherwise moves the mouse inside the view.
     * The user interaction needed to create selections will also change, and the user will be required
     * to hold down the shift key to create a selection or perform other mouse activities inside the 
     * view area - since the program running in the terminal is being allowed to handle normal mouse
     * events itself.
     *
     * @param usesMouse Set to true if the program running in the terminal is interested in mouse events
     * or false otherwise.
     */
    void setUsesMouse(bool usesMouse);
    
    /** 
     * Shows a notification that a bell event has occurred in the terminal.
     * TODO: More documentation here
     */
    void bell(const QString& message);

Q_SIGNALS:

    /**
     * Emitted when the user presses a key whilst the terminal widget has focus.
     */
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
    void changedHistoryCursor( int value);
    void configureRequest( TerminalDisplay*, int state, int x, int y );

   void isBusySelecting(bool);
   void sendStringToEmu(const char*);

protected:

    virtual void styleChange( QStyle& );

    bool eventFilter( QObject *, QEvent * );
    bool event( QEvent * );

	//draws a string of normal text characters
	//The painter's font and other attributes needed to draw the string
	//should be set before drawTextFixed is called.
    void drawTextFixed(QPainter &paint, int x, int y,
                       QString& str, const Character *attr);

	//draws a string of line-drawing characters
	void drawLineCharString(QPainter& painter, int x, int y, const QString& str, const Character* attributes);

    void drawAttrStr(QPainter &paint, const QRect& rect,
                     QString& str, const Character *attr, bool pm, bool clear);
    
    void paintEvent( QPaintEvent * );

    void paintContents(QPainter &paint, const QRect &rect);
    void paintFilters(QPainter& painter);

    void showEvent(QShowEvent*);
    void hideEvent(QHideEvent*);
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
    //Renables bell noises and visuals.  Used to disable further bells for a short period of time
    //after emitting the first in a sequence of bell events.
    void enableBell();

private Q_SLOTS:
    void drop_menu_activated(QAction*);
    void swapColorTable();
    void tripleClickTimeout();  // resets possibleTripleClick

private:

    // maps a point on the widget to the position ( ie. line and column ) of the character
    // at that point.
    void characterPosition(QPoint widgetPoint,int& line,int& column);

    // shows a notification window in the middle of the widget indicating the terminal's
    // current size in columns and lines
    void showResizeNotification();

    // shows the popup menu associated with a hotspot
    void showHotSpotMenu(Filter::HotSpot* spot , const QPoint& position);

    // scrolls the image by a number of lines.  'lines' may be positive ( to scroll the image down ) 
    // or negative ( to scroll the image up )
    void scrollImage(int lines);

    void calcGeometry();
    void propagateSize();
    void updateImageSize();
    void makeImage();
    
    // the window onto the terminal screen which this display
    // is currently showing.  
    ScreenWindow* _screenWindow;

    bool _allowBell;

    QGridLayout* _gridLayout;

    bool _fixedFont; // has fixed pitch
    int  _fontHeight;     // height
    int  _fontWidth;     // width
    int  _fontAscent;     // ascend

    int _bX;    // offset
    int _bY;    // offset

    int _lines;      // the number of lines that can be displayed in the widget
    int _columns;    // the number of columns that can be displayed in the widget
    
    int _usedLines;  // the number of lines that are actually being used, this will be less
                    // than 'lines' if the character image provided with setImage() is smaller
                    // than the maximum image size which can be displayed

    int _usedColumns; // the number of columns that are actually being used, this will be less
                     // than 'columns' if the character image provided with setImage() is smaller
                     // than the maximum image size which can be displayed
    
    int _contentHeight;
    int _contentWidth;
    Character* _image; // [lines][columns]
               // only the area [usedLines][usedColumns] in the image contains valid data

    int _imageSize;
    QVector<LineProperty> _lineProperties;

    ColorEntry _colorTable[TABLE_COLORS];
    QColor _defaultBgColor;

    bool _resizing;
    bool _terminalSizeHint;
    bool _terminalSizeStartup;
    bool _bidiEnabled;
    bool _mouseMarks;

    QPoint  _iPntSel; // initial selection point
    QPoint  _pntSel; // current selection point
    QPoint  _tripleSelBegin; // help avoid flicker
    int     _actSel; // selection state
    bool    _wordSelectionMode;
    bool    _lineSelectionMode;
    bool    _preserveLineBreaks;
    bool    _columnSelectionMode;

    QClipboard*  _clipboard;
    QScrollBar* _scrollBar;
    ScrollBarLocation _scrollbarLocation;
    QString     _wordCharacters;
    int         _bellMode;

    bool _blinking;   // hide text in paintEvent
    bool _hasBlinker; // has characters to blink
    bool _cursorBlinking;     // hide cursor in paintEvent
    bool _hasBlinkingCursor;  // has blinking cursor enabled
    bool _ctrlDrag;           // require Ctrl key for drag
    bool _cutToBeginningOfLine; // triple click only selects forward
    bool _isPrinting; // Paint job is intended for printer
    bool _printerFriendly; // paint printer friendly, save ink
    bool _printerBold; // Use a bold font instead of overstrike for bold
    bool _isFixedSize; //Columns / lines are locked.
    QTimer* _blinkTimer;  // active when hasBlinker
    QTimer* _blinkCursorTimer;  // active when hasBlinkingCursor

    KMenu* _drop;
    QString _dropText;
    int _dndFileCount;

    bool _possibleTripleClick;  // is set in mouseDoubleClickEvent and deleted
                               // after QApplication::doubleClickInterval() delay

    static bool s_antialias;   // do we antialias or not
    static bool s_standalone;  // are we part of a standalone konsole?

    QFrame* _resizeWidget;
    QLabel* _resizeLabel;
    QTimer* _resizeTimer;

	//widgets related to the warning message that appears when the user presses Ctrl+S to suspend
	//terminal output - informing them what has happened and how to resume output
	QLabel* _outputSuspendedLabel; 
    	
    uint _lineSpacing;

    QRect       _cursorRect; //for quick changing of cursor

    QPoint _configureRequestPoint;  // remember right mouse button click position
    bool _colorsInverted; // true during visual bell

    // the rim should normally be 1, 0 only when running in full screen mode.
    int _rimX;      // left/right rim width
    int _rimY;      // top/bottom rim high
    QSize _size;
	
    QString _imPreeditText;
    int _imPreeditLength;
    int _imStart;
    int _imStartLine;
    int _imEnd;
    int _imSelStart;
    int _imSelEnd;
    int _cursorLine;
    int _cursorCol;
    bool _isIMEdit;
    bool _isIMSel;

    QRgb _blendColor;

    QAction* _pasteAction;
    QAction* _cdAction;
    QAction* _cpAction;
    QAction* _mvAction;
    QAction* _lnAction;

    TerminalImageFilterChain* _filterChain;
    QRect _mouseOverHotspotArea;

    KeyboardCursorShape _cursorShape;
    QColor _cursorColor;

	//the delay in milliseconds between redrawing blinking text
	static const int BLINK_DELAY = 500;
    static bool HAVE_TRANSPARENCY;

public:
    static void setTransparencyEnabled(bool enable)
    {
        HAVE_TRANSPARENCY = enable;
    }
};

};

#endif // TERMINALDISPLAY_H
