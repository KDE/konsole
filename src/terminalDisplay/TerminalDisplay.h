/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALDISPLAY_H
#define TERMINALDISPLAY_H

// Qt
#include <QColor>
#include <QPointer>
#include <QWidget>

#include <memory>

// Konsole
#include "../characters/Character.h"
#include "Enumeration.h"
#include "ScreenWindow.h"
#include "ScrollState.h"
#include "colorscheme/ColorScheme.h"
#include "konsoleprivate_export.h"
#include "widgets/TerminalHeaderBar.h"

#include "TerminalBell.h"

class QDrag;
class QDragEnterEvent;
class QDropEvent;
class QLabel;
class QTimer;
class QEvent;
class QVBoxLayout;
class QKeyEvent;
class QScroller;
class QShowEvent;
class QHideEvent;
class QTimerEvent;
class QScrollEvent;
class QScrollPrepareEvent;
class KMessageWidget;

namespace Konsole
{
class TerminalPainter;
class TerminalScrollBar;
class TerminalColor;
class TerminalFont;

class KonsolePrintManager;

class FilterChain;
class TerminalImageFilterChain;
class SessionController;
class IncrementalSearchBar;
class HotSpot;
class Profile;

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

    void showDragTarget(const QPoint &cursorPos);
    void hideDragTarget();

    void applyProfile(const QExplicitlySharedDataPointer<Profile> &profile);

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
    QSharedPointer<HotSpot> filterActions(const QPoint &position);

    /** Specifies whether or not the cursor can blink. */
    void setBlinkingCursorEnabled(bool blink);

    /** Specifies whether or not text can blink. */
    void setBlinkingTextEnabled(bool blink);

    void setSessionController(SessionController *controller);
    SessionController *sessionController();
    Session::Ptr session() const;

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
     * Returns the number of lines of text which can be displayed in the widget.
     *
     * This will depend upon the height of the widget and the current font.
     * See fontHeight()
     */
    int lines() const
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
    int columns() const
    {
        return _columns;
    }

    int usedColumns() const
    {
        return _usedColumns;
    }

    void setSize(int columns, int lines);
    void propagateSize();

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

    TerminalHeaderBar *headerBar() const
    {
        return _headerBar;
    }

    void resetCursor();

    QRect contentRect() const
    {
        return _contentRect;
    }
    bool openLinksByDirectClick() const
    {
        return _openLinksByDirectClick;
    }

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
    QPointer<ScreenWindow> screenWindow() const
    {
        return _screenWindow;
    }

    // Select the current line.
    void selectCurrentLine();

    /**
     * Selects everything in the terminal
     */
    void selectAll();

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

    /**
     * Set whether mouse tracking should be allowed, even if requested.
     * This is stored separately from if mouse tracking is enabled, in case the
     * user turns it on/off while mouse tracking is requested.
     */
    void setAllowMouseTracking(const bool allow);
    bool allowsMouseTracking() const;

    bool hasCompositeFocus() const
    {
        return _hasCompositeFocus;
    }

    // maps an area in the character image to an area on the widget
    QRect imageToWidget(const QRect &imageArea) const;
    QRect widgetToImage(const QRect &widgetArea) const;

    // maps a point on the widget to the position ( ie. line and column )
    // of the character at that point. When the edge is true, it maps to
    // a character which left edge is closest to the point.
    QPair<int, int> getCharacterPosition(const QPoint &widgetPoint, bool edge) const;

    // toggle the header bar Minimize/Maximize button.
    void setExpandedMode(bool expand);

    TerminalScrollBar *scrollBar() const
    {
        return _scrollBar;
    }

    TerminalColor *terminalColor() const
    {
        return _terminalColor;
    }

    TerminalFont *terminalFont() const
    {
        return _terminalFont.get();
    }

    /**
     * Return the current color scheme
     */
    const std::shared_ptr<const ColorScheme> &colorScheme() const
    {
        return _colorScheme;
    }

    bool cursorBlinking() const
    {
        return _cursorBlinking;
    }

    bool textBlinking() const
    {
        return _textBlinking;
    }

    Enum::CursorShapeEnum cursorShape() const
    {
        return _cursorShape;
    }

    bool bidiEnabled() const
    {
        return _bidiEnabled;
    }

    bool semanticUpDown() const
    {
        return _semanticUpDown;
    }

    int semanticHints() const
    {
        return _semanticHints;
    }

    ColorSchemeWallpaper::Ptr wallpaper() const
    {
        return _wallpaper;
    }

    struct InputMethodData {
        QString preeditString;
        QRect previousPreeditRect;
    };

    // returns true if the cursor's position is on display.
    bool isCursorOnDisplay() const;

    // returns the position of the cursor in columns and lines
    QPoint cursorPosition() const;

    // returns 0 - not selecting, 1 - pending selection (button pressed but no movement yet), 2 - selecting
    int selectionState() const;

    bool trimTrailingWhitespace() const
    {
        return _trimTrailingSpaces;
    }

    Qt::Edge droppedEdge() const
    {
        return _overlayEdge;
    }
    IncrementalSearchBar *searchBar() const;
    Character getCursorCharacter(int column, int line);

    int loc(int x, int y) const;

    /**
     * Scrolls current ScreenWindow
     *
     * it's needed for proper handling scroll commands in the Vt102Emulation class
     */
    void scrollScreenWindow(enum ScreenWindow::RelativeScrollMode mode, int amount);

    /**
     * Causes the widget to display or hide a message informing the user that terminal
     * output has been suspended (by using the flow control key combination Ctrl+S)
     *
     * @param suspended True if terminal output has been suspended and the warning message should
     *                     be shown or false to indicate that terminal output has been resumed and that
     *                     the warning message should disappear.
     */
    void outputSuspended(bool suspended);

    // Used to show/hide the message widget
    void updateReadOnlyState(bool readonly);

public Q_SLOTS:
    /**
     * Causes the terminal display to fetch the latest character image from the associated
     * terminal screen ( see setScreenWindow() ) and redraw the display.
     */
    void updateImage();

    /** Copies the selected text to the X11 Selection. */
    void copyToX11Selection();

    /** Copies the selected text to the system clipboard. */
    void copyToClipboard(Screen::DecodingOptions options = Screen::PlainText);

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

    void setBracketedPasteMode(bool on);

    /**
     * Shows a notification that a bell event has occurred in the terminal.
     * TODO: More documentation here
     */
    void bell(const QString &message);

    // Used for requestPrint
    void printScreen();

    void notificationClicked(const QString &xdgActivationToken);

Q_SIGNALS:
    void requestToggleExpansion();
    void requestMoveToNewTab(TerminalDisplay *display);

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

    void activationRequest(const QString &xdgActivationToken);
    void sendStringToEmu(const QByteArray &local8BitString);

    void compositeFocusChanged(bool focused);

    void peekPrimaryRequested(bool doPeek);

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
    void scrollPrepareEvent(QScrollPrepareEvent *);
    void scrollEvent(QScrollEvent *);

    void extendSelection(const QPoint &position);

    // drag and drop
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void doDrag();
    enum DragState {
        diNone,
        diPending,
        diDragging,
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

protected Q_SLOTS:

    void blinkTextEvent();
    void blinkCursorEvent();

private Q_SLOTS:

    void viewScrolledByUser();

private:
    Q_DISABLE_COPY(TerminalDisplay)

    // the area where the preedit string for input methods will be draw
    QRect preeditRect() const;

    // shows a notification window in the middle of the widget indicating the terminal's
    // current size in columns and lines
    void showResizeNotification();

    void calcGeometry();
    void updateImageSize();
    void makeImage();

    void paintFilters(QPainter &painter);

    void setupHeaderVisibility();

    // redraws the cursor
    void updateCursor();

    bool handleShortcutOverrideEvent(QKeyEvent *keyEvent);

    void doPaste(QString text, bool appendReturn);

    void processMidButtonClick(QMouseEvent *ev);

    QPoint findLineStart(const QPoint &pnt);
    QPoint findLineEnd(const QPoint &pnt);
    QPoint findWordStart(const QPoint &pnt);
    QPoint findWordEnd(const QPoint &pnt);

    /**
     * @param pnt A point, relative to this.
     * @return true if the given point is in the area with the main terminal content and not in some other widget.
     */
    bool isInTerminalRegion(const QPoint &pnt) const;

    // Uses the current settings for trimming whitespace and preserving linebreaks to create a proper flag value for Screen
    Screen::DecodingOptions currentDecodingOptions();

    // Boilerplate setup for MessageWidget
    KMessageWidget *createMessageWidget(const QString &text);

    // Some setters used by applyProfile
    void setAutoCopySelectedText(bool enabled);
    void setCopyTextAsHTML(bool enabled);
    void setMiddleClickPasteMode(Enum::MiddleClickPasteModeEnum mode);

    /**
     * Sets the display's contents margins.
     */
    void setMargin(int margin);

    /**
     * Sets whether the contents are centered between the margins.
     */
    void setCenterContents(bool enable);

    // the window onto the terminal screen which this display
    // is currently showing.
    QPointer<ScreenWindow> _screenWindow;

    bool _bellMasked;

    QVBoxLayout *_verticalLayout = nullptr;

    int _lines = 1; // the number of lines that can be displayed in the widget
    int _columns = 1; // the number of columns that can be displayed in the widget

    // Character line and character column as per a previous call to
    // getCharacterPosition() in mouseMoveEvent().
    int _prevCharacterLine = -1;
    int _prevCharacterColumn = -1;

    int _usedLines = 1; // the number of lines that are actually being used, this will be less
    // than 'lines' if the character image provided with setImage() is smaller
    // than the maximum image size which can be displayed

    int _usedColumns = 1; // the number of columns that are actually being used, this will be less
    // than 'columns' if the character image provided with setImage() is smaller
    // than the maximum image size which can be displayed

    QRect _contentRect;
    Character *_image = nullptr; // [lines][columns]
    // only the area [usedLines][usedColumns] in the image contains valid data

    int _imageSize = 0;
    QVector<LineProperty> _lineProperties;

    QColor _colorTable[TABLE_COLORS];

    uint _randomSeed = 0;

    bool _resizing = false;
    bool _showTerminalSizeHint = true;
    bool _bidiEnabled = false;
    bool _usesMouseTracking = false;
    bool _allowMouseTracking = true;
    bool _bracketedPasteMode = false;

    QPoint _iPntSel; // initial selection point
    QPoint _pntSel; // current selection point
    QPoint _tripleSelBegin; // help avoid flicker
    int _actSel = 0; // selection state
    bool _wordSelectionMode = false;
    bool _lineSelectionMode = false;
    bool _preserveLineBreaks = true;
    bool _columnSelectionMode = false;

    bool _autoCopySelectedText = false;
    bool _copyTextAsHTML = true;
    Enum::MiddleClickPasteModeEnum _middleClickPasteMode = Enum::PasteFromX11Selection;

    QString _wordCharacters;
    TerminalBell _bell{Enum::NotifyBell};

    bool _allowBlinkingText = true; // allow text to blink
    bool _allowBlinkingCursor = false; // allow cursor to blink
    bool _textBlinking = false; // text is blinking, hide it when drawing
    bool _cursorBlinking = false; // cursor is blinking, hide it when drawing
    bool _hasTextBlinker = false; // has characters to blink
    QTimer *_blinkTextTimer = nullptr;
    QTimer *_blinkCursorTimer = nullptr;

    bool _openLinksByDirectClick = false; // Open URL and hosts by single mouse click

    bool _ctrlRequiredForDrag = true; // require Ctrl key for drag selected text
    bool _dropUrlsAsText = false; // always paste URLs as text without showing copy/move menu

    Enum::TripleClickModeEnum _tripleClickMode = Enum::SelectWholeLine;
    bool _possibleTripleClick = false; // is set in mouseDoubleClickEvent and deleted
    // after QApplication::doubleClickInterval() delay

    QLabel *_resizeWidget = nullptr;
    QTimer *_resizeTimer = nullptr;

    bool _flowControlWarningEnabled = false;

    // widgets related to the warning message that appears when the user presses Ctrl+S to suspend
    // terminal output - informing them what has happened and how to resume output
    KMessageWidget *_outputSuspendedMessageWidget = nullptr;

    QSize _size;

    std::shared_ptr<const ColorScheme> _colorScheme;
    ColorSchemeWallpaper::Ptr _wallpaper;

    // list of filters currently applied to the display.  used for links and
    // search highlight
    TerminalImageFilterChain *_filterChain = nullptr;
    bool _filterUpdateRequired = true;

    Enum::CursorShapeEnum _cursorShape = Enum::BlockCursor;

    InputMethodData _inputMethodData;

    // the delay in milliseconds between redrawing blinking text
    static const int TEXT_BLINK_DELAY = 500;

    // the duration of the size hint in milliseconds
    static const int SIZE_HINT_DURATION = 1000;

    SessionController *_sessionController = nullptr;

    bool _trimLeadingSpaces = false; // trim leading spaces in selected text
    bool _trimTrailingSpaces = false; // trim trailing spaces in selected text
    bool _mouseWheelZoom = false; // enable mouse wheel zooming or not

    int _margin = 1; // the contents margin
    bool _centerContents = false; // center the contents between margins

    KMessageWidget *_readOnlyMessageWidget = nullptr; // Message shown at the top when read-only mode gets activated

    // Needed to know whether the mode really changed between update calls
    bool _readOnly = false;

    bool _dimWhenInactive = false;
    int _dimValue;

    ScrollState _scrollWheelState;
    IncrementalSearchBar *_searchBar = nullptr;
    TerminalHeaderBar *_headerBar = nullptr;
    QRect _searchResultRect;
    friend class TerminalDisplayAccessible;

    bool _drawOverlay = false;
    Qt::Edge _overlayEdge;

    bool _hasCompositeFocus;
    bool _displayVerticalLine = false;
    int _displayVerticalLineAtChar = 80;

    QKeySequence _peekPrimaryShortcut;

    TerminalPainter *_terminalPainter = nullptr;
    TerminalScrollBar *_scrollBar = nullptr;
    TerminalColor *_terminalColor = nullptr;
    std::unique_ptr<TerminalFont> _terminalFont;

    std::unique_ptr<KonsolePrintManager> _printManager;

    int _semanticHints;
    bool _semanticUpDown;
    bool _semanticInputClick;
};

}

#endif // TERMINALDISPLAY_H
