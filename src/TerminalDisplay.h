/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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
#include <QtGui/QColor>
#include <QtCore/QPointer>
#include <QWidget>

// Konsole
#include "Character.h"
#include "konsole_export.h"
#include "ScreenWindow.h"
#include "ColorScheme.h"
#include "Enumeration.h"

class QDrag;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QTimer;
class QEvent;
class QGridLayout;
class QKeyEvent;
class QScrollBar;
class QShowEvent;
class QHideEvent;
class QTimerEvent;

namespace Konsole
{
class FilterChain;
class TerminalImageFilterChain;
class SessionController;
/**
 * A widget which displays output from a terminal emulation and sends input keypresses and mouse activity
 * to the terminal.
 *
 * When the terminal emulation receives new output from the program running in the terminal,
 * it will update the display by calling updateImage().
 *
 * TODO More documentation
 */
class KONSOLEPRIVATE_EXPORT TerminalDisplay : public QWidget
{
    Q_OBJECT

public:
    /** Constructs a new terminal display widget with the specified parent. */
    explicit TerminalDisplay(QWidget* parent = 0);
    virtual ~TerminalDisplay();

    /** Returns the terminal color palette used by the display. */
    const ColorEntry* colorTable() const;
    /** Sets the terminal color palette used by the display. */
    void setColorTable(const ColorEntry table[]);
    /**
     * Sets the seed used to generate random colors for the display
     * (in color schemes that support them).
     */
    void setRandomSeed(uint seed);
    /**
     * Returns the seed used to generate random colors for the display
     * (in color schemes that support them).
     */
    uint randomSeed() const;

    /** Sets the opacity of the terminal display. */
    void setOpacity(qreal opacity);

    /** Sets the background picture */
    void setWallpaper(ColorSchemeWallpaper::Ptr p);

    /**
     * Specifies whether the terminal display has a vertical scroll bar, and if so whether it
     * is shown on the left or right side of the display.
     */
    void setScrollBarPosition(Enum::ScrollBarPositionEnum position);
    Enum::ScrollBarPositionEnum scrollBarPosition() const {
        return _scrollbarLocation;
    }

    /**
     * Sets the current position and range of the display's scroll bar.
     *
     * @param cursor The position of the scroll bar's thumb.
     * @param lines The maximum value of the scroll bar.
     */
    void setScroll(int cursor, int lines);

    void setScrollFullPage(bool fullPage);
    bool scrollFullPage() const;

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

    /**
     * Returns a list of menu actions created by the filters for the content
     * at the given @p position.
     */
    QList<QAction*> filterActions(const QPoint& position);

    /** Specifies whether or not the cursor can blink. */
    void setBlinkingCursorEnabled(bool blink);
    /** Returns true if the cursor is allowed to blink or false otherwise. */
    bool blinkingCursorEnabled() const {
        return _allowBlinkingCursor;
    }

    /** Specifies whether or not text can blink. */
    void setBlinkingTextEnabled(bool blink);

    void setControlDrag(bool enable) {
        _ctrlRequiredForDrag = enable;
    }
    bool ctrlRequiredForDrag() const {
        return _ctrlRequiredForDrag;
    }

    /** Sets how the text is selected when the user triple clicks within the display. */
    void setTripleClickMode(Enum::TripleClickModeEnum mode) {
        _tripleClickMode = mode;
    }
    /** See setTripleClickSelectionMode() */
    Enum::TripleClickModeEnum tripleClickMode() const {
        return _tripleClickMode;
    }

    /**
     * Specifies whether links and email addresses should be underlined when
     * hovered by the mouse. Defaults to true.
     */
    void setUnderlineLinks(bool value) {
        _underlineLinks = value;
    }
    /**
     * Returns true if links and email addresses should be underlined when
     * hovered by the mouse.
     */
    bool getUnderlineLinks() const {
        return _underlineLinks;
    }

    /**
     * Specifies whether links and email addresses should be opened when
     * clicked with the mouse. Defaults to false.
     */
    void setOpenLinksByDirectClick(bool value) {
        _openLinksByDirectClick = value;
    }
    /**
     * Returns true if links and email addresses should be opened when
     * clicked with the mouse.
     */
    bool getOpenLinksByDirectClick() const {
        return _openLinksByDirectClick;
    }

    /**
     * Sets whether trailing spaces should be trimmed in selected text.
     */
    void setTrimTrailingSpaces(bool enabled) {
        _trimTrailingSpaces = enabled;
    }

    /**
     * Returns true if trailing spaces should be trimmed in selected text.
     */
    bool trimTrailingSpaces() const {
        return _trimTrailingSpaces;
    }

    void setLineSpacing(uint);
    uint lineSpacing() const;

    void setSessionController(SessionController* controller);
    SessionController* sessionController();

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
    void setKeyboardCursorShape(Enum::CursorShapeEnum shape);
    /**
     * Returns the shape of the keyboard cursor.  See setKeyboardCursorShape()
     */
    Enum::CursorShapeEnum keyboardCursorShape() const;

    /**
     * Sets the color used to draw the keyboard cursor.
     *
     * The keyboard cursor defaults to using the foreground color of the character
     * underneath it.
     *
     * @param color By default, the widget uses the color of the
     * character under the cursor to draw the cursor, and inverts the
     * color of that character to make sure it is still readable. If @p
     * color is a valid QColor, the widget uses that color to draw the
     * cursor. If @p color is not an valid QColor, the widget falls back
     * to the default behavior.
     */
    void setKeyboardCursorColor(const QColor& color);

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
    int  lines() const {
        return _lines;
    }
    /**
     * Returns the number of characters of text which can be displayed on
     * each line in the widget.
     *
     * This will depend upon the width of the widget and the current font.
     * See fontWidth()
     */
    int  columns() const {
        return _columns;
    }

    /**
     * Returns the height of the characters in the font used to draw the text in the display.
     */
    int  fontHeight() const {
        return _fontHeight;
    }
    /**
     * Returns the width of the characters in the display.
     * This assumes the use of a fixed-width font.
     */
    int  fontWidth() const {
        return _fontWidth;
    }

    void setSize(int columns, int lines);

    // reimplemented
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
    void setWordCharacters(const QString& wc);
    /**
     * Returns the characters which are considered part of a word for the
     * purpose of selecting words in the display with the mouse.
     *
     * @see setWordCharacters()
     */
    QString wordCharacters() const {
        return _wordCharacters;
    }

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
    int bellMode() const;

    /** Play a visual bell for prompt or warning. */
    void visualBell();

    /**
     * Specified whether zoom terminal on Ctrl+mousewheel  is enabled or not.
     * Defaults to enabled.
     */
    void setMouseWheelZoom(bool value) {
        _mouseWheelZoom = value;
    };
    /**
     * Returns the whether zoom terminal on Ctrl+mousewheel is enabled.
     *
     * See setMouseWheelZoom()
     */
    bool mouseWheelZoom() {
        return _mouseWheelZoom;
    };

    /**
     * Reimplemented.  Has no effect.  Use setVTFont() to change the font
     * used to draw characters in the display.
     */
    virtual void setFont(const QFont &);

    /** Returns the font used to draw characters in the display */
    QFont getVTFont() {
        return font();
    }

    /**
     * Sets the font used to draw the display.  Has no effect if @p font
     * is larger than the size of the display itself.
     */
    void setVTFont(const QFont& font);

    /** Increases the font size */
    void increaseFontSize();

    /** Decreases the font size */
    void decreaseFontSize();

    /**
     * Specified whether anti-aliasing of text in the terminal display
     * is enabled or not.  Defaults to enabled.
     */
    void setAntialias(bool value) {
        _antialiasText = value;
    }
    /**
     * Returns true if anti-aliasing of text in the terminal is enabled.
     */
    bool antialias() const {
        return _antialiasText;
    }

    /**
     * Specifies whether characters with intense colors should be rendered
     * as bold. Defaults to true.
     */
    void setBoldIntense(bool value) {
        _boldIntense = value;
    }
    /**
     * Returns true if characters with intense colors are rendered in bold.
     */
    bool getBoldIntense() const {
        return _boldIntense;
    }

    /**
     * Sets whether or not the current height and width of the
     * terminal in lines and columns is displayed whilst the widget
     * is being resized.
     */
    void setShowTerminalSizeHint(bool on) {
        _showTerminalSizeHint = on;
    }
    /**
     * Returns whether or not the current height and width of
     * the terminal in lines and columns is displayed whilst the widget
     * is being resized.
     */
    bool showTerminalSizeHint() const {
        return _showTerminalSizeHint;
    }

    /**
     * Sets the status of the BiDi rendering inside the terminal display.
     * Defaults to disabled.
     */
    void setBidiEnabled(bool set) {
        _bidiEnabled = set;
    }
    /**
     * Returns the status of the BiDi rendering in this widget.
     */
    bool isBidiEnabled() const {
        return _bidiEnabled;
    }

    /**
     * Sets the terminal screen section which is displayed in this widget.
     * When updateImage() is called, the display fetches the latest character image from the
     * the associated terminal screen window.
     *
     * In terms of the model-view paradigm, the ScreenWindow is the model which is rendered
     * by the TerminalDisplay.
     */
    void setScreenWindow(ScreenWindow* window);
    /** Returns the terminal screen section which is displayed in this widget.  See setScreenWindow() */
    ScreenWindow* screenWindow() const;

    // Select the current line.
    void selectCurrentLine();

    void printContent(QPainter& painter, bool friendly);

public slots:
    /**
     * Scrolls current ScreenWindow
     *
     * it's needed for proper handling scroll commands in the Vt102Emulation class
     */
    void scrollScreenWindow(enum ScreenWindow::RelativeScrollMode mode , int amount);

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

    void setAutoCopySelectedText(bool enabled);

    void setMiddleClickPasteMode(Enum::MiddleClickPasteModeEnum mode);

    /** Copies the selected text to the X11 Selection. */
    void copyToX11Selection();

    /** Copies the selected text to the system clipboard. */
    void copyToClipboard();

    /**
     * Pastes the content of the clipboard into the
     * display.
     */
    void pasteFromClipboard(bool appendEnter = false);
    /**
     * Pastes the content of the X11 selection into the
     * display.
     */
    void pasteFromX11Selection(bool appendEnter = false);

    /**
       * Changes whether the flow control warning box should be shown when the flow control
       * stop key (Ctrl+S) are pressed.
       */
    void setFlowControlWarningEnabled(bool enabled);
    /**
     * Returns true if the flow control warning box is enabled.
     * See outputSuspended() and setFlowControlWarningEnabled()
     */
    bool flowControlWarningEnabled() const {
        return _flowControlWarningEnabled;
    }

    /**
     * Causes the widget to display or hide a message informing the user that terminal
     * output has been suspended (by using the flow control key combination Ctrl+S)
     *
     * @param suspended True if terminal output has been suspended and the warning message should
     *                     be shown or false to indicate that terminal output has been resumed and that
     *                     the warning message should disappear.
     */
    void outputSuspended(bool suspended);

    /**
     * Sets whether the program whose output is being displayed in the view
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

    /** See setUsesMouse() */
    bool usesMouse() const;

    void setBracketedPasteMode(bool bracketedPasteMode);
    bool bracketedPasteMode() const;

    /**
     * Shows a notification that a bell event has occurred in the terminal.
     * TODO: More documentation here
     */
    void bell(const QString& message);

    /**
     * Gets the background of the display
     * @see setBackgroundColor(), setColorTable(), setForegroundColor()
     */
    QColor getBackgroundColor() const;

    /**
     * Sets the background of the display to the specified color.
     * @see setColorTable(), getBackgroundColor(), setForegroundColor()
     */
    void setBackgroundColor(const QColor& color);

    /**
     * Sets the text of the display to the specified color.
     * @see setColorTable(), setBackgroundColor(), getBackgroundColor()
     */
    void setForegroundColor(const QColor& color);

    /**
     * Sets the display's contents margins.
     */
    void setMargin(int margin);

    /**
     * Sets whether the contents are centered between the margins.
     */
    void setCenterContents(bool enable);

signals:

    /**
     * Emitted when the user presses a key whilst the terminal widget has focus.
     */
    void keyPressedSignal(QKeyEvent* event);

    /**
     * A mouse event occurred.
     * @param button The mouse button (0 for left button, 1 for middle button, 2 for right button, 3 for release)
     * @param column The character column where the event occurred
     * @param line The character row where the event occurred
     * @param eventType The type of event.  0 for a mouse press / release or 1 for mouse motion
     */
    void mouseSignal(int button, int column, int line, int eventType);
    void changedFontMetricSignal(int height, int width);
    void changedContentSizeSignal(int height, int width);

    /**
     * Emitted when the user right clicks on the display, or right-clicks with the Shift
     * key held down if usesMouse() is true.
     *
     * This can be used to display a context menu.
     */
    void configureRequest(const QPoint& position);

    /**
     * When a shortcut which is also a valid terminal key sequence is pressed while
     * the terminal widget  has focus, this signal is emitted to allow the host to decide
     * whether the shortcut should be overridden.
     * When the shortcut is overridden, the key sequence will be sent to the terminal emulation instead
     * and the action associated with the shortcut will not be triggered.
     *
     * @p override is set to false by default and the shortcut will be triggered as normal.
     */
    void overrideShortcutCheck(QKeyEvent* keyEvent, bool& override);

    void sendStringToEmu(const char*);

protected:
    virtual bool event(QEvent* event);

    virtual void paintEvent(QPaintEvent* event);

    virtual void showEvent(QShowEvent* event);
    virtual void hideEvent(QHideEvent* event);
    virtual void resizeEvent(QResizeEvent* event);

    virtual void contextMenuEvent(QContextMenuEvent* event);

    virtual void fontChange(const QFont&);
    virtual void focusInEvent(QFocusEvent* event);
    virtual void focusOutEvent(QFocusEvent* event);
    virtual void keyPressEvent(QKeyEvent* event);
    virtual void leaveEvent(QEvent* event);
    virtual void mouseDoubleClickEvent(QMouseEvent* event);
    virtual void mousePressEvent(QMouseEvent* event);
    virtual void mouseReleaseEvent(QMouseEvent* event);
    virtual void mouseMoveEvent(QMouseEvent* event);
    virtual void extendSelection(const QPoint& pos);
    virtual void wheelEvent(QWheelEvent* event);

    virtual bool focusNextPrevChild(bool next);

    // drag and drop
    virtual void dragEnterEvent(QDragEnterEvent* event);
    virtual void dropEvent(QDropEvent* event);
    void doDrag();
    enum DragState { diNone, diPending, diDragging };

    struct DragInfo {
        DragState       state;
        QPoint          start;
        QDrag*          dragObject;
    } _dragInfo;

    // classifies the 'ch' into one of three categories
    // and returns a character to indicate which category it is in
    //
    //     - A space (returns ' ')
    //     - Part of a word (returns 'a')
    //     - Other characters (returns the input character)
    QChar charClass(const Character& ch) const;

    void clearImage();

    void mouseTripleClickEvent(QMouseEvent* event);
    void selectLine(QPoint pos, bool entireLine);

    // reimplemented
    virtual void inputMethodEvent(QInputMethodEvent* event);
    virtual QVariant inputMethodQuery(Qt::InputMethodQuery query) const;

protected slots:

    void scrollBarPositionChanged(int value);
    void blinkTextEvent();
    void blinkCursorEvent();

private slots:

    void unmaskBell();
    void swapFGBGColors();
    void tripleClickTimeout();  // resets possibleTripleClick
    void viewScrolledByUser();

    /**
     * Called from the drag-and-drop popup. Causes the dropped URLs to be pasted as text.
     */
    void dropMenuPasteActionTriggered();

    void dropMenuCdActionTriggered();

    void dismissOutputSuspendedMessage();

private:
    // -- Drawing helpers --

    // divides the part of the display specified by 'rect' into
    // fragments according to their colors and styles and calls
    // drawTextFragment() or drawPrinterFriendlyTextFragment()
    // to draw the fragments
    void drawContents(QPainter& painter, const QRect& rect);
    // draw a transparent rectangle over the line of the current match
    void drawCurrentResultRect(QPainter& painter);
    // draws a section of text, all the text in this section
    // has a common color and style
    void drawTextFragment(QPainter& painter, const QRect& rect,
                          const QString& text, const Character* style);

    void drawPrinterFriendlyTextFragment(QPainter& painter, const QRect& rect,
                                         const QString& text, const Character* style);
    // draws the background for a text fragment
    // if useOpacitySetting is true then the color's alpha value will be set to
    // the display's transparency (set with setOpacity()), otherwise the background
    // will be drawn fully opaque
    void drawBackground(QPainter& painter, const QRect& rect, const QColor& color,
                        bool useOpacitySetting);
    // draws the cursor character
    void drawCursor(QPainter& painter, const QRect& rect , const QColor& foregroundColor,
                    const QColor& backgroundColor , bool& invertColors);
    // draws the characters or line graphics in a text fragment
    void drawCharacters(QPainter& painter, const QRect& rect,  const QString& text,
                        const Character* style, bool invertCharacterColor);
    // draws a string of line graphics
    void drawLineCharString(QPainter& painter, int x, int y,
                            const QString& str, const Character* attributes);

    // draws the preedit string for input methods
    void drawInputMethodPreeditString(QPainter& painter , const QRect& rect);

    // --

    // maps an area in the character image to an area on the widget
    QRect imageToWidget(const QRect& imageArea) const;

    // maps a point on the widget to the position ( ie. line and column )
    // of the character at that point.
    void getCharacterPosition(const QPoint& widgetPoint, int& line, int& column) const;

    // the area where the preedit string for input methods will be draw
    QRect preeditRect() const;

    // shows a notification window in the middle of the widget indicating the terminal's
    // current size in columns and lines
    void showResizeNotification();

    // scrolls the image by a number of lines.
    // 'lines' may be positive ( to scroll the image down )
    // or negative ( to scroll the image up )
    // 'region' is the part of the image to scroll - currently only
    // the top, bottom and height of 'region' are taken into account,
    // the left and right are ignored.
    void scrollImage(int lines , const QRect& region);

    void calcGeometry();
    void propagateSize();
    void updateImageSize();
    void makeImage();

    void paintFilters(QPainter& painter);

    // returns a region covering all of the areas of the widget which contain
    // a hotspot
    QRegion hotSpotRegion() const;

    // returns the position of the cursor in columns and lines
    QPoint cursorPosition() const;

    // redraws the cursor
    void updateCursor();

    bool handleShortcutOverrideEvent(QKeyEvent* event);

    void doPaste(QString text, bool appendReturn);

    void processMidButtonClick(QMouseEvent* event);

    QPoint findLineStart(const QPoint &pnt);
    QPoint findLineEnd(const QPoint &pnt);
    QPoint findWordStart(const QPoint &pnt);
    QPoint findWordEnd(const QPoint &pnt);

    // the window onto the terminal screen which this display
    // is currently showing.
    QPointer<ScreenWindow> _screenWindow;

    bool _bellMasked;

    QGridLayout* _gridLayout;

    bool _fixedFont; // has fixed pitch
    int  _fontHeight;     // height
    int  _fontWidth;     // width
    int  _fontAscent;     // ascend
    bool _boldIntense;   // Whether intense colors should be rendered with bold font

    int _leftMargin;    // offset
    int _topMargin;    // offset

    int _lines;      // the number of lines that can be displayed in the widget
    int _columns;    // the number of columns that can be displayed in the widget

    int _usedLines;  // the number of lines that are actually being used, this will be less
    // than 'lines' if the character image provided with setImage() is smaller
    // than the maximum image size which can be displayed

    int _usedColumns; // the number of columns that are actually being used, this will be less
    // than 'columns' if the character image provided with setImage() is smaller
    // than the maximum image size which can be displayed

    QRect _contentRect;
    Character* _image; // [lines][columns]
    // only the area [usedLines][usedColumns] in the image contains valid data

    int _imageSize;
    QVector<LineProperty> _lineProperties;

    ColorEntry _colorTable[TABLE_COLORS];
    uint _randomSeed;

    bool _resizing;
    bool _showTerminalSizeHint;
    bool _bidiEnabled;
    bool _mouseMarks;
    bool _bracketedPasteMode;

    QPoint  _iPntSel; // initial selection point
    QPoint  _pntSel; // current selection point
    QPoint  _tripleSelBegin; // help avoid flicker
    int     _actSel; // selection state
    bool    _wordSelectionMode;
    bool    _lineSelectionMode;
    bool    _preserveLineBreaks;
    bool    _columnSelectionMode;

    bool _autoCopySelectedText;
    Enum::MiddleClickPasteModeEnum _middleClickPasteMode;

    QScrollBar* _scrollBar;
    Enum::ScrollBarPositionEnum _scrollbarLocation;
    bool _scrollFullPage;
    QString     _wordCharacters;
    int         _bellMode;

    bool _allowBlinkingText;  // allow text to blink
    bool _allowBlinkingCursor;  // allow cursor to blink
    bool _textBlinking;   // text is blinking, hide it when drawing
    bool _cursorBlinking;     // cursor is blinking, hide it when drawing
    bool _hasTextBlinker; // has characters to blink
    QTimer* _blinkTextTimer;
    QTimer* _blinkCursorTimer;

    bool _underlineLinks;     // Underline URL and hosts on mouse hover
    bool _openLinksByDirectClick;     // Open URL and hosts by single mouse click

    bool _ctrlRequiredForDrag; // require Ctrl key for drag selected text

    Enum::TripleClickModeEnum _tripleClickMode;
    bool _possibleTripleClick;  // is set in mouseDoubleClickEvent and deleted
    // after QApplication::doubleClickInterval() delay

    QLabel* _resizeWidget;
    QTimer* _resizeTimer;

    bool _flowControlWarningEnabled;

    //widgets related to the warning message that appears when the user presses Ctrl+S to suspend
    //terminal output - informing them what has happened and how to resume output
    QLabel* _outputSuspendedLabel;

    uint _lineSpacing;

    QSize _size;

    QRgb _blendColor;

    ColorSchemeWallpaper::Ptr _wallpaper;

    // list of filters currently applied to the display.  used for links and
    // search highlight
    TerminalImageFilterChain* _filterChain;
    QRegion _mouseOverHotspotArea;

    Enum::CursorShapeEnum _cursorShape;

    // cursor color. If it is invalid (by default) then the foreground
    // color of the character under the cursor is used
    QColor _cursorColor;

    struct InputMethodData {
        QString preeditString;
        QRect previousPreeditRect;
    };
    InputMethodData _inputMethodData;

    bool _antialiasText;   // do we anti-alias or not

    bool _printerFriendly; // are we currently painting to a printer in black/white mode

    //the delay in milliseconds between redrawing blinking text
    static const int TEXT_BLINK_DELAY = 500;

    //the duration of the size hint in milliseconds
    static const int SIZE_HINT_DURATION = 1000;

    SessionController* _sessionController;

    bool _trimTrailingSpaces;   // trim trailing spaces in selected text
    bool _mouseWheelZoom;   // enable mouse wheel zooming or not

    int _margin;      // the contents margin
    bool _centerContents;   // center the contents between margins

    qreal _opacity;

    friend class TerminalDisplayAccessible;
};

class AutoScrollHandler : public QObject
{
    Q_OBJECT

public:
    explicit AutoScrollHandler(QWidget* parent);
protected:
    virtual void timerEvent(QTimerEvent* event);
    virtual bool eventFilter(QObject* watched, QEvent* event);
private:
    QWidget* widget() const {
        return static_cast<QWidget*>(parent());
    }
    int _timerId;
};
}

#endif // TERMINALDISPLAY_H
