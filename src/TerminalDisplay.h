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
#include <QColor>
#include <QPointer>
#include <QWidget>

// Konsole
#include "Character.h"
#include "konsoleprivate_export.h"
#include "ScreenWindow.h"
#include "ColorScheme.h"
#include "Enumeration.h"
#include "ScrollState.h"
#include "Profile.h"
#include "TerminalHeaderBar.h"
#include "Filter.h"

class QDrag;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QTimer;
class QEvent;
class QVBoxLayout;
class QKeyEvent;
class QScrollBar;
class QShowEvent;
class QHideEvent;
class QTimerEvent;
class KMessageWidget;

namespace Konsole {
class FilterChain;
class TerminalImageFilterChain;
class SessionController;
class IncrementalSearchBar;

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
    explicit TerminalDisplay(QWidget *parent = nullptr);
    ~TerminalDisplay() override;

    void showDragTarget(const QPoint& cursorPos);
    void hideDragTarget();

    void applyProfile(const Profile::Ptr& profile);

    /** Returns the terminal color palette used by the display. */
    const ColorEntry *colorTable() const;
    /** Sets the terminal color palette used by the display. */
    void setColorTable(const ColorEntry table[]);
    /**
     * Sets the seed used to generate random colors for the display
     * (in color schemes that support them).
     */
    void setRandomSeed(uint randomSeed);
    /**
     * Returns the seed used to generate random colors for the display
     * (in color schemes that support them).
     */
    uint randomSeed() const;

    /** Sets the opacity of the terminal display. */
    void setOpacity(qreal opacity);

    /** Sets the background picture */
    void setWallpaper(const ColorSchemeWallpaper::Ptr &p);

    /**
     * Specifies whether the terminal display has a vertical scroll bar, and if so whether it
     * is shown on the left or right side of the display.
     */
    void setScrollBarPosition(Enum::ScrollBarPositionEnum position);

    /**
     * Sets the current position and range of the display's scroll bar.
     *
     * @param cursor The position of the scroll bar's thumb.
     * @param slines The maximum value of the scroll bar.
     */
    void setScroll(int cursor, int slines);

    void setScrollFullPage(bool fullPage);
    bool scrollFullPage() const;
    void setHighlightScrolledLines(bool highlight);

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
    FilterChain *filterChain() const;

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
    QSharedPointer<Filter::HotSpot> filterActions(const QPoint &position);

    /** Specifies whether or not the cursor can blink. */
    void setBlinkingCursorEnabled(bool blink);

    /** Specifies whether or not text can blink. */
    void setBlinkingTextEnabled(bool blink);

    void setLineSpacing(uint);
    uint lineSpacing() const;

    void setSessionController(SessionController *controller);
    SessionController *sessionController();

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
     * Sets the Cursor Style (DECSCUSR) via escape sequences
     * @p shape cursor shape
     * @p isBlinking if true, the cursor will be set to blink
    */
    void setCursorStyle(Enum::CursorShapeEnum shape, bool isBlinking);
    /**
     * Resets the cursor style to the current profile cursor shape and
     * blinking settings
    */
    void resetCursorStyle();

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
    void setKeyboardCursorColor(const QColor &color);

    /**
     * Sets the color used to draw the character underneath the keyboard cursor.
     *
     * The keyboard cursor defaults to using the background color of the
     * terminal cell to draw the character at the cursor position.
     *
     * @param color By default, the widget uses the color of the
     * character under the cursor to draw the cursor, and inverts the
     * color of that character to make sure it is still readable. If @p
     * color is a valid QColor, the widget uses that color to draw the
     * character underneath the cursor. If @p color is not an valid QColor,
     * the widget falls back to the default behavior.
     */
    void setKeyboardCursorTextColor(const QColor &color);

    /**
     * Returns the number of lines of text which can be displayed in the widget.
     *
     * This will depend upon the height of the widget and the current font.
     * See fontHeight()
     */
    int  lines() const
    {
        return _lines;
    }

    /**
     * Returns the number of characters of text which can be displayed on
     * each line in the widget.
     *
     * This will depend upon the width of the widget and the current font.
     * See fontWidth()
     */
    int  columns() const
    {
        return _columns;
    }

    /**
     * Returns the height of the characters in the font used to draw the text in the display.
     */
    int  fontHeight() const
    {
        return _fontHeight;
    }

    /**
     * Returns the width of the characters in the display.
     * This assumes the use of a fixed-width font.
     */
    int  fontWidth() const
    {
        return _fontWidth;
    }

    void setSize(int columns, int lines);

    // reimplemented
    QSize sizeHint() const override;

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
    void setWordCharacters(const QString &wc);

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

    /** Returns the font used to draw characters in the display */
    QFont getVTFont()
    {
        return font();
    }

    TerminalHeaderBar *headerBar() const
    {
        return _headerBar;
    }
    /**
     * Sets the font used to draw the display.  Has no effect if @p font
     * is larger than the size of the display itself.
     */
    void setVTFont(const QFont &f);

    /** Increases the font size */
    void increaseFontSize();

    /** Decreases the font size */
    void decreaseFontSize();

    /** Reset the font size */
    void resetFontSize();

    /**
     * Sets the terminal screen section which is displayed in this widget.
     * When updateImage() is called, the display fetches the latest character image from the
     * the associated terminal screen window.
     *
     * In terms of the model-view paradigm, the ScreenWindow is the model which is rendered
     * by the TerminalDisplay.
     */
    void setScreenWindow(ScreenWindow *window);
    /** Returns the terminal screen section which is displayed in this widget.  See setScreenWindow() */
    ScreenWindow *screenWindow() const;

    // Select the current line.
    void selectCurrentLine();

    /**
     * Selects everything in the terminal
     */
    void selectAll();

    void printContent(QPainter &painter, bool friendly);

    /**
     * Gets the foreground of the display
     * @see setForegroundColor(), setColorTable(), setBackgroundColor()
     */
    QColor getForegroundColor() const;

    /**
     * Gets the background of the display
     * @see setBackgroundColor(), setColorTable(), setForegroundColor()
     */
    QColor getBackgroundColor() const;

    bool bracketedPasteMode() const;

    /**
     * Returns true if the flow control warning box is enabled.
     * See outputSuspended() and setFlowControlWarningEnabled()
     */
    bool flowControlWarningEnabled() const
    {
        return _flowControlWarningEnabled;
    }

    /** See setUsesMouseTracking() */
    bool usesMouseTracking() const;

    /** See setAlternateScrolling() */
    bool alternateScrolling() const;

    bool hasCompositeFocus() const
    {
        return _hasCompositeFocus;
    }

public Q_SLOTS:
    /**
     * Scrolls current ScreenWindow
     *
     * it's needed for proper handling scroll commands in the Vt102Emulation class
     */
    void scrollScreenWindow(enum ScreenWindow::RelativeScrollMode mode, int amount);

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

    void setCopyTextAsHTML(bool enabled);

    void setMiddleClickPasteMode(Enum::MiddleClickPasteModeEnum mode);

    /** Copies the selected text to the X11 Selection. */
    void copyToX11Selection();

    /** Copies the selected text to the system clipboard. */
    void copyToClipboard();

    /**
     * Pastes the content of the clipboard into the display.
     *
     * URLs of local files are treated specially:
     *  - The scheme part, "file://", is removed from each URL
     *  - The URLs are pasted as a space-separated list of file paths
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
    void setFlowControlWarningEnabled(bool enable);
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
     * Sets whether the program currently running in the terminal is interested
     * in Mouse Tracking events.
     *
     * When set to true, Konsole will send Mouse Tracking events.
     *
     * The user interaction needed to create text selections will change
     * also, and the user will be required to hold down the Shift key to
     * create a selection or perform other mouse activities inside the view
     * area, since the program running in the terminal is being allowed
     * to handle normal mouse events itself.
     *
     * @param on Set to true if the program running in the terminal is
     * interested in Mouse Tracking events or false otherwise.
     */
    void setUsesMouseTracking(bool on);

    /**
     * Sets the AlternateScrolling profile property which controls whether
     * to emulate up/down key presses for mouse scroll wheel events.
     * For more details, check the documentation of that property in the
     * Profile header.
     * Enabled by default.
     */
    void setAlternateScrolling(bool enable);

    void setBracketedPasteMode(bool on);

    /**
     * Shows a notification that a bell event has occurred in the terminal.
     * TODO: More documentation here
     */
    void bell(const QString &message);

    /**
     * Sets the background of the display to the specified color.
     * @see setColorTable(), getBackgroundColor(), setForegroundColor()
     */
    void setBackgroundColor(const QColor &color);

    /**
     * Sets the text of the display to the specified color.
     * @see setColorTable(), setBackgroundColor(), getBackgroundColor()
     */
    void setForegroundColor(const QColor &color);

    /**
     * Sets the display's contents margins.
     */
    void setMargin(int margin);

    /**
     * Sets whether the contents are centered between the margins.
     */
    void setCenterContents(bool enable);

    /**
     * Return the current color scheme
     */
    ColorScheme const *colorScheme() const
    {
        return _colorScheme;
    }

    Enum::ScrollBarPositionEnum scrollBarPosition() const
    {
        return _scrollbarLocation;
    }

    Qt::Edge droppedEdge() const {
        return _overlayEdge;
    }

    // Used to show/hide the message widget
    void updateReadOnlyState(bool readonly);
    IncrementalSearchBar *searchBar() const;
Q_SIGNALS:
    void requestToggleExpansion();
    /**
     * Emitted when the user presses a key whilst the terminal widget has focus.
     */
    void keyPressedSignal(QKeyEvent *event);

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
     * Emitted when the user right clicks on the display, or right-clicks
     * with the Shift key held down if usesMouseTracking() is true.
     *
     * This can be used to display a context menu.
     */
    void configureRequest(const QPoint &position);

    /**
     * When a shortcut which is also a valid terminal key sequence is pressed while
     * the terminal widget  has focus, this signal is emitted to allow the host to decide
     * whether the shortcut should be overridden.
     * When the shortcut is overridden, the key sequence will be sent to the terminal emulation instead
     * and the action associated with the shortcut will not be triggered.
     *
     * @p override is set to false by default and the shortcut will be triggered as normal.
     */
    void overrideShortcutCheck(QKeyEvent *keyEvent, bool &override);

    void sendStringToEmu(const QByteArray &local8BitString);

    void compositeFocusChanged(bool focused);

protected:
    // events
    bool event(QEvent *event) override;
    void paintEvent(QPaintEvent *pe) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void wheelEvent(QWheelEvent *ev) override;
    bool focusNextPrevChild(bool next) override;

    void fontChange(const QFont &);
    void extendSelection(const QPoint &position);

    // drag and drop
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void doDrag();
    enum DragState {
        diNone, diPending, diDragging
    };

    struct DragInfo {
        DragState state;
        QPoint start;
        QDrag *dragObject;
    } _dragInfo;

    // classifies the 'ch' into one of three categories
    // and returns a character to indicate which category it is in
    //
    //     - A space (returns ' ')
    //     - Part of a word (returns 'a')
    //     - Other characters (returns the input character)
    QChar charClass(const Character &ch) const;

    void clearImage();

    void mouseTripleClickEvent(QMouseEvent *ev);
    void selectLine(QPoint pos, bool entireLine);

    // reimplemented
    void inputMethodEvent(QInputMethodEvent *event) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery query) const override;

    void onColorsChanged();

protected Q_SLOTS:

    void scrollBarPositionChanged(int value);
    void blinkTextEvent();
    void blinkCursorEvent();
    void highlightScrolledLinesEvent();

private Q_SLOTS:

    void swapFGBGColors();
    void viewScrolledByUser();

    void dismissOutputSuspendedMessage();

private:
    Q_DISABLE_COPY(TerminalDisplay)

    // -- Drawing helpers --

    // divides the part of the display specified by 'rect' into
    // fragments according to their colors and styles and calls
    // drawTextFragment() or drawPrinterFriendlyTextFragment()
    // to draw the fragments
    void drawContents(QPainter &painter, const QRect &rect);
    // draw a transparent rectangle over the line of the current match
    void drawCurrentResultRect(QPainter &painter);
    // draw a thin highlight on the left of the screen for lines that have been scrolled into view
    void highlightScrolledLines(QPainter& painter);
    // compute which region need to be repainted for scrolled lines highlight
    QRegion highlightScrolledLinesRegion(bool nothingChanged);

    // draws a section of text, all the text in this section
    // has a common color and style
    void drawTextFragment(QPainter &painter, const QRect &rect, const QString &text,
                          const Character *style);

    void drawPrinterFriendlyTextFragment(QPainter &painter, const QRect &rect, const QString &text,
                                         const Character *style);
    // draws the background for a text fragment
    // if useOpacitySetting is true then the color's alpha value will be set to
    // the display's transparency (set with setOpacity()), otherwise the background
    // will be drawn fully opaque
    void drawBackground(QPainter &painter, const QRect &rect, const QColor &backgroundColor,
                        bool useOpacitySetting);
    // draws the cursor character
    void drawCursor(QPainter &painter, const QRect &rect, const QColor &foregroundColor,
                    const QColor &backgroundColor, QColor &characterColor);
    // draws the characters or line graphics in a text fragment
    void drawCharacters(QPainter &painter, const QRect &rect, const QString &text,
                        const Character *style, const QColor &characterColor);
    // draws a string of line graphics
    void drawLineCharString(QPainter &painter, int x, int y, const QString &str,
                            const Character *attributes);

    // draws the preedit string for input methods
    void drawInputMethodPreeditString(QPainter &painter, const QRect &rect);

    // --

    // maps an area in the character image to an area on the widget
    QRect imageToWidget(const QRect &imageArea) const;
    QRect widgetToImage(const QRect &widgetArea) const;

    // maps a point on the widget to the position ( ie. line and column )
    // of the character at that point. When the edge is true, it maps to
    // a character which left edge is closest to the point.
    void getCharacterPosition(const QPoint &widgetPoint, int &line, int &column, bool edge) const;

    // the area where the preedit string for input methods will be draw
    QRect preeditRect() const;

    // shows a notification window in the middle of the widget indicating the terminal's
    // current size in columns and lines
    void showResizeNotification();

    // applies changes to scrollbarLocation to the scroll bar and
    // if @propagate is true, propagates size information
    void applyScrollBarPosition(bool propagate = true);

    // scrolls the image by a number of lines.
    // 'lines' may be positive ( to scroll the image down )
    // or negative ( to scroll the image up )
    // 'region' is the part of the image to scroll - currently only
    // the top, bottom and height of 'region' are taken into account,
    // the left and right are ignored.
    void scrollImage(int lines, const QRect &screenWindowRegion);

    void calcGeometry();
    void propagateSize();
    void updateImageSize();
    void makeImage();

    void paintFilters(QPainter &painter);

    void setupHeaderVisibility();

    // returns a region covering all of the areas of the widget which contain
    // a hotspot
    QRegion hotSpotRegion() const;

    // returns the position of the cursor in columns and lines
    QPoint cursorPosition() const;

    // returns true if the cursor's position is on display.
    bool isCursorOnDisplay() const;

    // redraws the cursor
    void updateCursor();

    bool handleShortcutOverrideEvent(QKeyEvent *keyEvent);

    void doPaste(QString text, bool appendReturn);

    void processMidButtonClick(QMouseEvent *ev);

    QPoint findLineStart(const QPoint &pnt);
    QPoint findLineEnd(const QPoint &pnt);
    QPoint findWordStart(const QPoint &pnt);
    QPoint findWordEnd(const QPoint &pnt);

    // Uses the current settings for trimming whitespace and preserving linebreaks to create a proper flag value for Screen
    Screen::DecodingOptions currentDecodingOptions();

    // Boilerplate setup for MessageWidget
    KMessageWidget* createMessageWidget(const QString &text);

    int loc(int x, int y) const;

    // the window onto the terminal screen which this display
    // is currently showing.
    QPointer<ScreenWindow> _screenWindow;

    bool _bellMasked;

    QVBoxLayout *_verticalLayout;

    bool _fixedFont; // has fixed pitch
    int _fontHeight;      // height
    int _fontWidth;      // width
    int _fontAscent;      // ascend
    bool _boldIntense;   // Whether intense colors should be rendered with bold font

    int _lines;      // the number of lines that can be displayed in the widget
    int _columns;    // the number of columns that can be displayed in the widget

    int _usedLines;  // the number of lines that are actually being used, this will be less
    // than 'lines' if the character image provided with setImage() is smaller
    // than the maximum image size which can be displayed

    int _usedColumns; // the number of columns that are actually being used, this will be less
    // than 'columns' if the character image provided with setImage() is smaller
    // than the maximum image size which can be displayed

    QRect _contentRect;
    Character *_image; // [lines][columns]
    // only the area [usedLines][usedColumns] in the image contains valid data

    int _imageSize;
    QVector<LineProperty> _lineProperties;

    ColorEntry _colorTable[TABLE_COLORS];

    uint _randomSeed;

    bool _resizing;
    bool _showTerminalSizeHint;
    bool _bidiEnabled;
    bool _usesMouseTracking;
    bool _alternateScrolling;
    bool _bracketedPasteMode;

    QPoint _iPntSel;  // initial selection point
    QPoint _pntSel;  // current selection point
    QPoint _tripleSelBegin;  // help avoid flicker
    int _actSel;     // selection state
    bool _wordSelectionMode;
    bool _lineSelectionMode;
    bool _preserveLineBreaks;
    bool _columnSelectionMode;

    bool _autoCopySelectedText;
    bool _copyTextAsHTML;
    Enum::MiddleClickPasteModeEnum _middleClickPasteMode;

    QScrollBar *_scrollBar;
    Enum::ScrollBarPositionEnum _scrollbarLocation;
    bool _scrollFullPage;
    QString _wordCharacters;
    int _bellMode;

    bool _allowBlinkingText;  // allow text to blink
    bool _allowBlinkingCursor;  // allow cursor to blink
    bool _textBlinking;   // text is blinking, hide it when drawing
    bool _cursorBlinking;     // cursor is blinking, hide it when drawing
    bool _hasTextBlinker; // has characters to blink
    QTimer *_blinkTextTimer;
    QTimer *_blinkCursorTimer;

    Qt::KeyboardModifiers _urlHintsModifiers;
    bool _showUrlHint;
    bool _reverseUrlHints;
    bool _openLinksByDirectClick;     // Open URL and hosts by single mouse click

    bool _ctrlRequiredForDrag; // require Ctrl key for drag selected text
    bool _dropUrlsAsText; // always paste URLs as text without showing copy/move menu

    Enum::TripleClickModeEnum _tripleClickMode;
    bool _possibleTripleClick;  // is set in mouseDoubleClickEvent and deleted
    // after QApplication::doubleClickInterval() delay

    QLabel *_resizeWidget;
    QTimer *_resizeTimer;

    bool _flowControlWarningEnabled;

    //widgets related to the warning message that appears when the user presses Ctrl+S to suspend
    //terminal output - informing them what has happened and how to resume output
    KMessageWidget *_outputSuspendedMessageWidget;

    uint _lineSpacing;

    QSize _size;

    QRgb _blendColor;

    ColorScheme const* _colorScheme;
    ColorSchemeWallpaper::Ptr _wallpaper;

    // list of filters currently applied to the display.  used for links and
    // search highlight
    TerminalImageFilterChain *_filterChain;
    QRegion _mouseOverHotspotArea;
    bool _filterUpdateRequired;

    Enum::CursorShapeEnum _cursorShape;

    // cursor color. If it is invalid (by default) then the foreground
    // color of the character under the cursor is used
    QColor _cursorColor;

    // cursor text color. If it is invalid (by default) then the background
    // color of the character under the cursor is used
    QColor _cursorTextColor;

    struct InputMethodData {
        QString preeditString;
        QRect previousPreeditRect;
    };
    InputMethodData _inputMethodData;

    bool _antialiasText;   // do we anti-alias or not
    bool _useFontLineCharacters;

    bool _printerFriendly; // are we currently painting to a printer in black/white mode

    //the delay in milliseconds between redrawing blinking text
    static const int TEXT_BLINK_DELAY = 500;

    //the duration of the size hint in milliseconds
    static const int SIZE_HINT_DURATION = 1000;

    SessionController *_sessionController;

    bool _trimLeadingSpaces;   // trim leading spaces in selected text
    bool _trimTrailingSpaces;   // trim trailing spaces in selected text
    bool _mouseWheelZoom;   // enable mouse wheel zooming or not

    int _margin;      // the contents margin
    bool _centerContents;   // center the contents between margins

    KMessageWidget *_readOnlyMessageWidget; // Message shown at the top when read-only mode gets activated

    // Needed to know whether the mode really changed between update calls
    bool _readOnly;

    qreal _opacity;

    bool _dimWhenInactive;

    ScrollState _scrollWheelState;
    IncrementalSearchBar *_searchBar;
    TerminalHeaderBar *_headerBar;
    QRect _searchResultRect;
    friend class TerminalDisplayAccessible;

    bool _drawOverlay;
    Qt::Edge _overlayEdge;

    struct {
        bool enabled = false;
        QRect rect;
        int previousScrollCount = 0;
        QTimer *timer = nullptr;
        bool needToClear = false;
    } _highlightScrolledLinesControl;
    static const int HIGHLIGHT_SCROLLED_LINES_WIDTH = 3;

    bool _hasCompositeFocus;

    QSharedPointer<Filter::HotSpot> _currentlyHoveredHotspot;
};

class AutoScrollHandler : public QObject
{
    Q_OBJECT

public:
    explicit AutoScrollHandler(QWidget *parent);
protected:
    void timerEvent(QTimerEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
private:
    QWidget *widget() const
    {
        return static_cast<QWidget *>(parent());
    }

    int _timerId;
};

// Watches compositeWidget and all its focusable children,
// and emits focusChanged() signal when either compositeWidget's
// or a child's focus changed.
// Limitation: children added after the object was created
// will not be registered.
class CompositeWidgetFocusWatcher : public QObject
{
    Q_OBJECT

public:
    explicit CompositeWidgetFocusWatcher(QWidget *compositeWidget, QObject *parent);
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;

Q_SIGNALS:
    void compositeFocusChanged(bool focused);

private:
    void registerWidgetAndChildren(QWidget *widget);

    QWidget *_compositeWidget;
};

}

#endif // TERMINALDISPLAY_H
