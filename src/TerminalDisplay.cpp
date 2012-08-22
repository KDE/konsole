/*
    This file is part of Konsole, a terminal emulator for KDE.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>
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

// Own
#include "TerminalDisplay.h"

// Qt
#include <QApplication>
#include <QtGui/QClipboard>
#include <QtGui/QKeyEvent>
#include <QtCore/QEvent>
#include <QtCore/QFileInfo>
#include <QGridLayout>
#include <QAction>
#include <QLabel>
#include <QtGui/QPainter>
#include <QtGui/QPixmap>
#include <QScrollBar>
#include <QStyle>
#include <QtCore/QTimer>
#include <QToolTip>
#include <QtGui/QAccessible>

// KDE
#include <KShell>
#include <KColorScheme>
#include <KCursor>
#include <KDebug>
#include <KLocalizedString>
#include <KNotification>
#include <KGlobalSettings>
#include <KIO/NetAccess>
#include <konq_operations.h>
#include <KFileItem>

// Konsole
#include "Filter.h"
#include "konsole_wcwidth.h"
#include "TerminalCharacterDecoder.h"
#include "Screen.h"
#include "ScreenWindow.h"
#include "LineFont.h"
#include "SessionController.h"
#include "ExtendedCharTable.h"
#include "TerminalDisplayAccessible.h"
#include "SessionManager.h"
#include "Session.h"

using namespace Konsole;

#ifndef loc
#define loc(X,Y) ((Y)*_columns+(X))
#endif

#define REPCHAR   "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "abcdefgjijklmnopqrstuvwxyz" \
    "0123456789./+@"

// we use this to force QPainter to display text in LTR mode
// more information can be found in: http://unicode.org/reports/tr9/
const QChar LTR_OVERRIDE_CHAR(0x202D);

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Colors                                     */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/* Note that we use ANSI color order (bgr), while IBMPC color order is (rgb)

   Code        0       1       2       3       4       5       6       7
   ----------- ------- ------- ------- ------- ------- ------- ------- -------
   ANSI  (bgr) Black   Red     Green   Yellow  Blue    Magenta Cyan    White
   IBMPC (rgb) Black   Blue    Green   Cyan    Red     Magenta Yellow  White
*/

ScreenWindow* TerminalDisplay::screenWindow() const
{
    return _screenWindow;
}
void TerminalDisplay::setScreenWindow(ScreenWindow* window)
{
    // disconnect existing screen window if any
    if (_screenWindow) {
        disconnect(_screenWindow , 0 , this , 0);
    }

    _screenWindow = window;

    if (_screenWindow) {
        connect(_screenWindow , SIGNAL(outputChanged()) , this , SLOT(updateLineProperties()));
        connect(_screenWindow , SIGNAL(outputChanged()) , this , SLOT(updateImage()));
        _screenWindow->setWindowLines(_lines);
    }
}

const ColorEntry* TerminalDisplay::colorTable() const
{
    return _colorTable;
}
void TerminalDisplay::setBackgroundColor(const QColor& color)
{
    _colorTable[DEFAULT_BACK_COLOR].color = color;

    QPalette p = palette();
    p.setColor(backgroundRole(), color);
    setPalette(p);

    // Avoid propagating the palette change to the scroll bar
    _scrollBar->setPalette(QApplication::palette());

    update();
}
QColor TerminalDisplay::getBackgroundColor() const
{
    QPalette p = palette();
    return p.color(backgroundRole());
}
void TerminalDisplay::setForegroundColor(const QColor& color)
{
    _colorTable[DEFAULT_FORE_COLOR].color = color;

    update();
}
void TerminalDisplay::setColorTable(const ColorEntry table[])
{
    for (int i = 0; i < TABLE_COLORS; i++)
        _colorTable[i] = table[i];

    setBackgroundColor(_colorTable[DEFAULT_BACK_COLOR].color);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                   Font                                    */
/*                                                                           */
/* ------------------------------------------------------------------------- */

static inline bool isLineCharString(const QString& string)
{
    if (string.length() == 0)
        return false;

    return isSupportedLineChar(string.at(0).unicode());
}

void TerminalDisplay::fontChange(const QFont&)
{
    QFontMetrics fm(font());
    _fontHeight = fm.height() + _lineSpacing;

    // waba TerminalDisplay 1.123:
    // "Base character width on widest ASCII character. This prevents too wide
    //  characters in the presence of double wide (e.g. Japanese) characters."
    // Get the width from representative normal width characters
    _fontWidth = qRound((double)fm.width(REPCHAR) / (double)qstrlen(REPCHAR));

    _fixedFont = true;

    const int fw = fm.width(REPCHAR[0]);
    for (unsigned int i = 1; i < qstrlen(REPCHAR); i++) {
        if (fw != fm.width(REPCHAR[i])) {
            _fixedFont = false;
            break;
        }
    }

    if (_fontWidth < 1)
        _fontWidth = 1;

    _fontAscent = fm.ascent();

    emit changedFontMetricSignal(_fontHeight, _fontWidth);
    propagateSize();
    update();
}

void TerminalDisplay::setVTFont(const QFont& f)
{
    QFont font = f;

    QFontMetrics metrics(font);

    if (!QFontInfo(font).fixedPitch()) {
        kWarning() << "Using an unsupported variable-width font in the terminal.  This may produce display errors.";
    }

    if (metrics.height() < height() && metrics.maxWidth() < width()) {
        // hint that text should be drawn without anti-aliasing.
        // depending on the user's font configuration, this may not be respected
        if (!_antialiasText)
            font.setStyleStrategy(QFont::NoAntialias);

        // experimental optimization.  Konsole assumes that the terminal is using a
        // mono-spaced font, in which case kerning information should have an effect.
        // Disabling kerning saves some computation when rendering text.
        font.setKerning(false);

        // Konsole cannot handle non-integer font metrics
        font.setStyleStrategy(QFont::StyleStrategy(font.styleStrategy() | QFont::ForceIntegerMetrics));

        QWidget::setFont(font);
        fontChange(font);
    }
}

void TerminalDisplay::setFont(const QFont &)
{
    // ignore font change request if not coming from konsole itself
}

void TerminalDisplay::increaseFontSize()
{
    QFont font = getVTFont();
    font.setPointSizeF(font.pointSizeF() + 1);
    setVTFont(font);
}

void TerminalDisplay::decreaseFontSize()
{
    const qreal MinimumFontSize = 6;

    QFont font = getVTFont();
    font.setPointSizeF(qMax(font.pointSizeF() - 1, MinimumFontSize));
    setVTFont(font);
}

uint TerminalDisplay::lineSpacing() const
{
    return _lineSpacing;
}

void TerminalDisplay::setLineSpacing(uint i)
{
    _lineSpacing = i;
    setVTFont(font()); // Trigger an update.
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Accessibility                                     */
/*                                                                           */
/* ------------------------------------------------------------------------- */

namespace Konsole
{
/**
 * This function installs the factory function which lets Qt instanciate the QAccessibleInterface
 * for the TerminalDisplay.
 */
QAccessibleInterface* accessibleInterfaceFactory(const QString &key, QObject *object)
{
    Q_UNUSED(key)
    if (TerminalDisplay *display = qobject_cast<TerminalDisplay*>(object))
        return new TerminalDisplayAccessible(display);
    return 0;
}
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Constructor / Destructor                          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

TerminalDisplay::TerminalDisplay(QWidget* parent)
    : QWidget(parent)
    , _screenWindow(0)
    , _bellMasked(false)
    , _gridLayout(0)
    , _fontHeight(1)
    , _fontWidth(1)
    , _fontAscent(1)
    , _boldIntense(true)
    , _lines(1)
    , _columns(1)
    , _usedLines(1)
    , _usedColumns(1)
    , _contentHeight(1)
    , _contentWidth(1)
    , _image(0)
    , _randomSeed(0)
    , _resizing(false)
    , _showTerminalSizeHint(true)
    , _bidiEnabled(false)
    , _actSel(0)
    , _wordSelectionMode(false)
    , _lineSelectionMode(false)
    , _preserveLineBreaks(false)
    , _columnSelectionMode(false)
    , _autoCopySelectedText(false)
    , _middleClickPasteMode(Enum::PasteFromX11Selection)
    , _scrollbarLocation(Enum::ScrollBarRight)
    , _wordCharacters(":@-./_~")
    , _bellMode(Enum::NotifyBell)
    , _allowBlinkingText(true)
    , _allowBlinkingCursor(false)
    , _textBlinking(false)
    , _cursorBlinking(false)
    , _hasTextBlinker(false)
    , _underlineLinks(true)
    , _openLinksByDirectClick(false)
    , _isFixedSize(false)
    , _ctrlRequiredForDrag(true)
    , _tripleClickMode(Enum::SelectWholeLine)
    , _possibleTripleClick(false)
    , _resizeWidget(0)
    , _resizeTimer(0)
    , _flowControlWarningEnabled(false)
    , _outputSuspendedLabel(0)
    , _lineSpacing(0)
    , _blendColor(qRgba(0, 0, 0, 0xff))
    , _filterChain(new TerminalImageFilterChain())
    , _cursorShape(Enum::BlockCursor)
    , _antialiasText(true)
    , _printerFriendly(false)
    , _sessionController(0)
    , _trimTrailingSpaces(false)
{
    // terminal applications are not designed with Right-To-Left in mind,
    // so the layout is forced to Left-To-Right
    setLayoutDirection(Qt::LeftToRight);

    _topMargin = DEFAULT_TOP_MARGIN;
    _leftMargin = DEFAULT_LEFT_MARGIN;

    // create scroll bar for scrolling output up and down
    _scrollBar = new QScrollBar(this);
    // set the scroll bar's slider to occupy the whole area of the scroll bar initially
    setScroll(0, 0);
    _scrollBar->setCursor(Qt::ArrowCursor);
    connect(_scrollBar, SIGNAL(valueChanged(int)),
            this, SLOT(scrollBarPositionChanged(int)));

    // setup timers for blinking text
    _blinkTextTimer = new QTimer(this);
    _blinkTextTimer->setInterval(TEXT_BLINK_DELAY);
    connect(_blinkTextTimer, SIGNAL(timeout()), this, SLOT(blinkTextEvent()));

    // setup timers for blinking cursor
    _blinkCursorTimer = new QTimer(this);
    _blinkCursorTimer->setInterval(QApplication::cursorFlashTime() / 2);
    connect(_blinkCursorTimer, SIGNAL(timeout()), this, SLOT(blinkCursorEvent()));

    // hide mouse cursor on keystroke or idle
    KCursor::setAutoHideCursor(this, true);
    setMouseTracking(true);

    setUsesMouse(true);

    setColorTable(ColorScheme::defaultTable);

    // Enable drag and drop support
    setAcceptDrops(true);
    _dragInfo.state = diNone;

    setFocusPolicy(Qt::WheelFocus);

    // enable input method support
    setAttribute(Qt::WA_InputMethodEnabled, true);

    // this is an important optimization, it tells Qt
    // that TerminalDisplay will handle repainting its entire area.
    setAttribute(Qt::WA_OpaquePaintEvent);

    _gridLayout = new QGridLayout(this);
    _gridLayout->setContentsMargins(0, 0, 0, 0);

    setLayout(_gridLayout);

    new AutoScrollHandler(this);


#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installFactory(Konsole::accessibleInterfaceFactory);
#endif
}

TerminalDisplay::~TerminalDisplay()
{
    disconnect(_blinkTextTimer);
    disconnect(_blinkCursorTimer);

    delete[] _image;

    delete _gridLayout;
    delete _outputSuspendedLabel;
    delete _filterChain;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Display Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/**
 A table for emulating the simple (single width) unicode drawing chars.
 It represents the 250x - 257x glyphs. If it's zero, we can't use it.
 if it's not, it's encoded as follows: imagine a 5x5 grid where the points are numbered
 0 to 24 left to top, top to bottom. Each point is represented by the corresponding bit.

 Then, the pixels basically have the following interpretation:
 _|||_
 -...-
 -...-
 -...-
 _|||_

where _ = none
      | = vertical line.
      - = horizontal line.
 */

enum LineEncode {
    TopL  = (1 << 1),
    TopC  = (1 << 2),
    TopR  = (1 << 3),

    LeftT = (1 << 5),
    Int11 = (1 << 6),
    Int12 = (1 << 7),
    Int13 = (1 << 8),
    RightT = (1 << 9),

    LeftC = (1 << 10),
    Int21 = (1 << 11),
    Int22 = (1 << 12),
    Int23 = (1 << 13),
    RightC = (1 << 14),

    LeftB = (1 << 15),
    Int31 = (1 << 16),
    Int32 = (1 << 17),
    Int33 = (1 << 18),
    RightB = (1 << 19),

    BotL  = (1 << 21),
    BotC  = (1 << 22),
    BotR  = (1 << 23)
};

static void drawLineChar(QPainter& paint, int x, int y, int w, int h, uchar code)
{
    //Calculate cell midpoints, end points.
    const int cx = x + w / 2;
    const int cy = y + h / 2;
    const int ex = x + w - 1;
    const int ey = y + h - 1;

    const quint32 toDraw = LineChars[code];

    //Top _lines:
    if (toDraw & TopL)
        paint.drawLine(cx - 1, y, cx - 1, cy - 2);
    if (toDraw & TopC)
        paint.drawLine(cx, y, cx, cy - 2);
    if (toDraw & TopR)
        paint.drawLine(cx + 1, y, cx + 1, cy - 2);

    //Bot _lines:
    if (toDraw & BotL)
        paint.drawLine(cx - 1, cy + 2, cx - 1, ey);
    if (toDraw & BotC)
        paint.drawLine(cx, cy + 2, cx, ey);
    if (toDraw & BotR)
        paint.drawLine(cx + 1, cy + 2, cx + 1, ey);

    //Left _lines:
    if (toDraw & LeftT)
        paint.drawLine(x, cy - 1, cx - 2, cy - 1);
    if (toDraw & LeftC)
        paint.drawLine(x, cy, cx - 2, cy);
    if (toDraw & LeftB)
        paint.drawLine(x, cy + 1, cx - 2, cy + 1);

    //Right _lines:
    if (toDraw & RightT)
        paint.drawLine(cx + 2, cy - 1, ex, cy - 1);
    if (toDraw & RightC)
        paint.drawLine(cx + 2, cy, ex, cy);
    if (toDraw & RightB)
        paint.drawLine(cx + 2, cy + 1, ex, cy + 1);

    //Intersection points.
    if (toDraw & Int11)
        paint.drawPoint(cx - 1, cy - 1);
    if (toDraw & Int12)
        paint.drawPoint(cx, cy - 1);
    if (toDraw & Int13)
        paint.drawPoint(cx + 1, cy - 1);

    if (toDraw & Int21)
        paint.drawPoint(cx - 1, cy);
    if (toDraw & Int22)
        paint.drawPoint(cx, cy);
    if (toDraw & Int23)
        paint.drawPoint(cx + 1, cy);

    if (toDraw & Int31)
        paint.drawPoint(cx - 1, cy + 1);
    if (toDraw & Int32)
        paint.drawPoint(cx, cy + 1);
    if (toDraw & Int33)
        paint.drawPoint(cx + 1, cy + 1);
}

void TerminalDisplay::drawLineCharString(QPainter& painter, int x, int y, const QString& str,
        const Character* attributes)
{
    const QPen& originalPen = painter.pen();

    if ((attributes->rendition & RE_BOLD) && _boldIntense) {
        QPen boldPen(originalPen);
        boldPen.setWidth(3);
        painter.setPen(boldPen);
    }

    for (int i = 0 ; i < str.length(); i++) {
        const uchar code = str[i].cell();
        if (LineChars[code])
            drawLineChar(painter, x + (_fontWidth * i), y, _fontWidth, _fontHeight, code);
    }

    painter.setPen(originalPen);
}

void TerminalDisplay::setKeyboardCursorShape(Enum::CursorShapeEnum shape)
{
    _cursorShape = shape;
}
Enum::CursorShapeEnum TerminalDisplay::keyboardCursorShape() const
{
    return _cursorShape;
}
void TerminalDisplay::setKeyboardCursorColor(const QColor& color)
{
    _cursorColor = color;
}
QColor TerminalDisplay::keyboardCursorColor() const
{
    return _cursorColor;
}

void TerminalDisplay::setOpacity(qreal opacity)
{
    QColor color(_blendColor);
    color.setAlphaF(opacity);

    // enable automatic background filling to prevent the display
    // flickering if there is no transparency
    /*if ( color.alpha() == 255 )
    {
        setAutoFillBackground(true);
    }
    else
    {
        setAutoFillBackground(false);
    }*/

    _blendColor = color.rgba();
}

void TerminalDisplay::setWallpaper(ColorSchemeWallpaper::Ptr p)
{
    _wallpaper = p;
}

void TerminalDisplay::drawBackground(QPainter& painter, const QRect& rect, const QColor& backgroundColor, bool useOpacitySetting)
{
    // the area of the widget showing the contents of the terminal display is drawn
    // using the background color from the color scheme set with setColorTable()
    //
    // the area of the widget behind the scroll-bar is drawn using the background
    // brush from the scroll-bar's palette, to give the effect of the scroll-bar
    // being outside of the terminal display and visual consistency with other KDE
    // applications.
    //
    QRect scrollBarArea = _scrollBar->isVisible() ?
                          rect.intersected(_scrollBar->geometry()) :
                          QRect();
    QRegion contentsRegion = QRegion(rect).subtracted(scrollBarArea);
    QRect contentsRect = contentsRegion.boundingRect();

    if (useOpacitySetting && !_wallpaper->isNull() &&
            _wallpaper->draw(painter, contentsRect)) {
    } else if (qAlpha(_blendColor) < 0xff && useOpacitySetting) {
        // TODO - On MacOS, using CompositionMode doesn't work.  Altering the
        //        transparency in the color scheme (appears to) alter the
        //        brightness(?).  I'm not sure #ifdef is worthwhile ATM.
        QColor color(backgroundColor);
        color.setAlpha(qAlpha(_blendColor));

        painter.save();
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(contentsRect, color);
        painter.restore();
    } else {
        painter.fillRect(contentsRect, backgroundColor);
    }

    painter.fillRect(scrollBarArea, _scrollBar->palette().background());
}

void TerminalDisplay::drawCursor(QPainter& painter,
                                 const QRect& rect,
                                 const QColor& foregroundColor,
                                 const QColor& /*backgroundColor*/,
                                 bool& invertCharacterColor)
{
    // don't draw cursor which is currently blinking
    if (_cursorBlinking)
        return;

    QRect cursorRect = rect;
    cursorRect.setHeight(_fontHeight - _lineSpacing - 1);

    QColor cursorColor = _cursorColor.isValid() ? _cursorColor : foregroundColor;
    painter.setPen(cursorColor);

    if (_cursorShape == Enum::BlockCursor) {
        // draw the cursor outline, adjusting the area so that
        // it is draw entirely inside 'rect'
        int penWidth = qMax(1, painter.pen().width());
        painter.drawRect(cursorRect.adjusted(penWidth / 2,
                                             penWidth / 2,
                                             - penWidth / 2 - penWidth % 2,
                                             - penWidth / 2 - penWidth % 2));

        // draw the cursor body only when the widget has focus
        if (hasFocus()) {
            painter.fillRect(cursorRect, cursorColor);

            if (!_cursorColor.isValid()) {
                // invert the color used to draw the text to ensure that the character at
                // the cursor position is readable
                invertCharacterColor = true;
            }
        }
    } else if (_cursorShape == Enum::UnderlineCursor)
        painter.drawLine(cursorRect.left(),
                         cursorRect.bottom(),
                         cursorRect.right(),
                         cursorRect.bottom());

    else if (_cursorShape == Enum::IBeamCursor)
        painter.drawLine(cursorRect.left(),
                         cursorRect.top(),
                         cursorRect.left(),
                         cursorRect.bottom());
}

void TerminalDisplay::drawCharacters(QPainter& painter,
                                     const QRect& rect,
                                     const QString& text,
                                     const Character* style,
                                     bool invertCharacterColor)
{
    // don't draw text which is currently blinking
    if (_textBlinking && (style->rendition & RE_BLINK))
        return;

    // setup bold and underline
    bool useBold;
    ColorEntry::FontWeight weight = style->fontWeight(_colorTable);
    if (weight == ColorEntry::UseCurrentFormat)
        useBold = ((style->rendition & RE_BOLD) && _boldIntense) || font().bold();
    else
        useBold = (weight == ColorEntry::Bold) ? true : false;
    const bool useUnderline = style->rendition & RE_UNDERLINE || font().underline();

    QFont font = painter.font();
    if (font.bold() != useBold
            || font.underline() != useUnderline) {
        font.setBold(useBold);
        font.setUnderline(useUnderline);
        painter.setFont(font);
    }

    // setup pen
    const CharacterColor& textColor = (invertCharacterColor ? style->backgroundColor : style->foregroundColor);
    const QColor color = textColor.color(_colorTable);
    QPen pen = painter.pen();
    if (pen.color() != color) {
        pen.setColor(color);
        painter.setPen(color);
    }

    // draw text
    if (isLineCharString(text)) {
        drawLineCharString(painter, rect.x(), rect.y(), text, style);
    } else {
        // Force using LTR as the document layout for the terminal area, because
        // there is no use cases for RTL emulator and RTL terminal application.
        //
        // This still allows RTL characters to be rendered in the RTL way.
        painter.setLayoutDirection(Qt::LeftToRight);

        // the drawText(rect,flags,string) overload is used here with null flags
        // instead of drawText(rect,string) because the (rect,string) overload causes
        // the application's default layout direction to be used instead of
        // the widget-specific layout direction, which should always be
        // Qt::LeftToRight for this widget
        //
        // This was discussed in: http://lists.kde.org/?t=120552223600002&r=1&w=2
        if (_bidiEnabled) {
            painter.drawText(rect, 0, text);
        } else {
            // See bug 280896 for more info
#if QT_VERSION >= 0x040800
            painter.drawText(rect, Qt::AlignBottom, LTR_OVERRIDE_CHAR + text);
#else
            painter.drawText(rect, 0, LTR_OVERRIDE_CHAR + text);
#endif
        }
    }
}

void TerminalDisplay::drawTextFragment(QPainter& painter ,
                                       const QRect& rect,
                                       const QString& text,
                                       const Character* style)
{
    painter.save();

    // setup painter
    const QColor foregroundColor = style->foregroundColor.color(_colorTable);
    const QColor backgroundColor = style->backgroundColor.color(_colorTable);

    // draw background if different from the display's background color
    if (backgroundColor != palette().background().color())
        drawBackground(painter, rect, backgroundColor,
                       false /* do not use transparency */);

    // draw cursor shape if the current character is the cursor
    // this may alter the foreground and background colors
    bool invertCharacterColor = false;
    if (style->rendition & RE_CURSOR)
        drawCursor(painter, rect, foregroundColor, backgroundColor, invertCharacterColor);

    // draw text
    drawCharacters(painter, rect, text, style, invertCharacterColor);

    painter.restore();
}

void TerminalDisplay::drawPrinterFriendlyTextFragment(QPainter& painter,
        const QRect& rect,
        const QString& text,
        const Character* style)
{
    painter.save();

    // Set the colors used to draw to black foreground and white
    // background for printer friendly output when printing
    Character print_style = *style;
    print_style.foregroundColor = CharacterColor(COLOR_SPACE_RGB, 0x00000000);
    print_style.backgroundColor = CharacterColor(COLOR_SPACE_RGB, 0xFFFFFFFF);

    // draw text
    drawCharacters(painter, rect, text, &print_style, false);

    painter.restore();
}

void TerminalDisplay::setRandomSeed(uint randomSeed)
{
    _randomSeed = randomSeed;
}
uint TerminalDisplay::randomSeed() const
{
    return _randomSeed;
}

// scrolls the image by 'lines', down if lines > 0 or up otherwise.
//
// the terminal emulation keeps track of the scrolling of the character
// image as it receives input, and when the view is updated, it calls scrollImage()
// with the final scroll amount.  this improves performance because scrolling the
// display is much cheaper than re-rendering all the text for the
// part of the image which has moved up or down.
// Instead only new lines have to be drawn
void TerminalDisplay::scrollImage(int lines , const QRect& screenWindowRegion)
{
    // if the flow control warning is enabled this will interfere with the
    // scrolling optimizations and cause artifacts.  the simple solution here
    // is to just disable the optimization whilst it is visible
    if (_outputSuspendedLabel && _outputSuspendedLabel->isVisible())
        return;

    // constrain the region to the display
    // the bottom of the region is capped to the number of lines in the display's
    // internal image - 2, so that the height of 'region' is strictly less
    // than the height of the internal image.
    QRect region = screenWindowRegion;
    region.setBottom(qMin(region.bottom(), this->_lines - 2));

    // return if there is nothing to do
    if (lines == 0
            || _image == 0
            || !region.isValid()
            || (region.top() + abs(lines)) >= region.bottom()
            || this->_lines <= region.height()) return;

    // hide terminal size label to prevent it being scrolled
    if (_resizeWidget && _resizeWidget->isVisible())
        _resizeWidget->hide();

    // Note:  With Qt 4.4 the left edge of the scrolled area must be at 0
    // to get the correct (newly exposed) part of the widget repainted.
    //
    // The right edge must be before the left edge of the scroll bar to
    // avoid triggering a repaint of the entire widget, the distance is
    // given by SCROLLBAR_CONTENT_GAP
    //
    // Set the QT_FLUSH_PAINT environment variable to '1' before starting the
    // application to monitor repainting.
    //
    const int scrollBarWidth = _scrollBar->isHidden() ? 0 : _scrollBar->width();
    const int SCROLLBAR_CONTENT_GAP = 1;
    QRect scrollRect;
    if (_scrollbarLocation == Enum::ScrollBarLeft) {
        scrollRect.setLeft(scrollBarWidth + SCROLLBAR_CONTENT_GAP);
        scrollRect.setRight(width());
    } else {
        scrollRect.setLeft(0);
        scrollRect.setRight(width() - scrollBarWidth - SCROLLBAR_CONTENT_GAP);
    }
    void* firstCharPos = &_image[ region.top() * this->_columns ];
    void* lastCharPos = &_image[(region.top() + abs(lines)) * this->_columns ];

    const int top = _topMargin + (region.top() * _fontHeight);
    const int linesToMove = region.height() - abs(lines);
    const int bytesToMove = linesToMove * this->_columns * sizeof(Character);

    Q_ASSERT(linesToMove > 0);
    Q_ASSERT(bytesToMove > 0);

    //scroll internal image
    if (lines > 0) {
        // check that the memory areas that we are going to move are valid
        Q_ASSERT((char*)lastCharPos + bytesToMove <
                 (char*)(_image + (this->_lines * this->_columns)));

        Q_ASSERT((lines * this->_columns) < _imageSize);

        //scroll internal image down
        memmove(firstCharPos , lastCharPos , bytesToMove);

        //set region of display to scroll
        scrollRect.setTop(top);
    } else {
        // check that the memory areas that we are going to move are valid
        Q_ASSERT((char*)firstCharPos + bytesToMove <
                 (char*)(_image + (this->_lines * this->_columns)));

        //scroll internal image up
        memmove(lastCharPos , firstCharPos , bytesToMove);

        //set region of the display to scroll
        scrollRect.setTop(top + abs(lines) * _fontHeight);
    }
    scrollRect.setHeight(linesToMove * _fontHeight);

    Q_ASSERT(scrollRect.isValid() && !scrollRect.isEmpty());

    //scroll the display vertically to match internal _image
    scroll(0 , _fontHeight * (-lines) , scrollRect);
}

QRegion TerminalDisplay::hotSpotRegion() const
{
    QRegion region;
    foreach(Filter::HotSpot * hotSpot , _filterChain->hotSpots()) {
        QRect r;
        if (hotSpot->startLine() == hotSpot->endLine()) {
            r.setLeft(hotSpot->startColumn());
            r.setTop(hotSpot->startLine());
            r.setRight(hotSpot->endColumn());
            r.setBottom(hotSpot->endLine());
            region |= imageToWidget(r);
        } else {
            r.setLeft(hotSpot->startColumn());
            r.setTop(hotSpot->startLine());
            r.setRight(_columns);
            r.setBottom(hotSpot->startLine());
            region |= imageToWidget(r);
            for (int line = hotSpot->startLine() + 1 ; line < hotSpot->endLine() ; line++) {
                r.setLeft(0);
                r.setTop(line);
                r.setRight(_columns);
                r.setBottom(line);
                region |= imageToWidget(r);
            }
            r.setLeft(0);
            r.setTop(hotSpot->endLine());
            r.setRight(hotSpot->endColumn());
            r.setBottom(hotSpot->endLine());
            region |= imageToWidget(r);
        }
    }
    return region;
}

void TerminalDisplay::processFilters()
{
    if (!_screenWindow)
        return;

    QRegion preUpdateHotSpots = hotSpotRegion();

    // use _screenWindow->getImage() here rather than _image because
    // other classes may call processFilters() when this display's
    // ScreenWindow emits a scrolled() signal - which will happen before
    // updateImage() is called on the display and therefore _image is
    // out of date at this point
    _filterChain->setImage(_screenWindow->getImage(),
                           _screenWindow->windowLines(),
                           _screenWindow->windowColumns(),
                           _screenWindow->getLineProperties());
    _filterChain->process();

    QRegion postUpdateHotSpots = hotSpotRegion();

    update(preUpdateHotSpots | postUpdateHotSpots);
}

void TerminalDisplay::updateImage()
{
    if (!_screenWindow)
        return;

    // optimization - scroll the existing image where possible and
    // avoid expensive text drawing for parts of the image that
    // can simply be moved up or down
    if (_wallpaper->isNull()) {
        scrollImage(_screenWindow->scrollCount() ,
                    _screenWindow->scrollRegion());
        _screenWindow->resetScrollCount();
    }

    if (!_image) {
        // Create _image.
        // The emitted changedContentSizeSignal also leads to getImage being recreated, so do this first.
        updateImageSize();
    }

    Character* const newimg = _screenWindow->getImage();
    const int lines = _screenWindow->windowLines();
    const int columns = _screenWindow->windowColumns();

    setScroll(_screenWindow->currentLine() , _screenWindow->lineCount());

    Q_ASSERT(this->_usedLines <= this->_lines);
    Q_ASSERT(this->_usedColumns <= this->_columns);

    int y, x, len;

    const QPoint tL  = contentsRect().topLeft();
    const int    tLx = tL.x();
    const int    tLy = tL.y();
    _hasTextBlinker = false;

    CharacterColor cf;       // undefined

    const int linesToUpdate = qMin(this->_lines, qMax(0, lines));
    const int columnsToUpdate = qMin(this->_columns, qMax(0, columns));

    char* dirtyMask = new char[columnsToUpdate + 2];
    QRegion dirtyRegion;

    // debugging variable, this records the number of lines that are found to
    // be 'dirty' ( ie. have changed from the old _image to the new _image ) and
    // which therefore need to be repainted
    int dirtyLineCount = 0;

    for (y = 0; y < linesToUpdate; ++y) {
        const Character* currentLine = &_image[y * this->_columns];
        const Character* const newLine = &newimg[y * columns];

        bool updateLine = false;

        // The dirty mask indicates which characters need repainting. We also
        // mark surrounding neighbours dirty, in case the character exceeds
        // its cell boundaries
        memset(dirtyMask, 0, columnsToUpdate + 2);

        for (x = 0 ; x < columnsToUpdate ; ++x) {
            if (newLine[x] != currentLine[x]) {
                dirtyMask[x] = true;
            }
        }

        if (!_resizing) // not while _resizing, we're expecting a paintEvent
            for (x = 0; x < columnsToUpdate; ++x) {
                _hasTextBlinker |= (newLine[x].rendition & RE_BLINK);

                // Start drawing if this character or the next one differs.
                // We also take the next one into account to handle the situation
                // where characters exceed their cell width.
                if (dirtyMask[x]) {
                    if (!newLine[x + 0].character)
                        continue;
                    const bool lineDraw = newLine[x + 0].isLineChar();
                    const bool doubleWidth = (x + 1 == columnsToUpdate) ? false : (newLine[x + 1].character == 0);
                    const quint8 cr = newLine[x].rendition;
                    const CharacterColor clipboard = newLine[x].backgroundColor;
                    if (newLine[x].foregroundColor != cf) cf = newLine[x].foregroundColor;
                    const int lln = columnsToUpdate - x;
                    for (len = 1; len < lln; ++len) {
                        const Character& ch = newLine[x + len];

                        if (!ch.character)
                            continue; // Skip trailing part of multi-col chars.

                        const bool nextIsDoubleWidth = (x + len + 1 == columnsToUpdate) ? false : (newLine[x + len + 1].character == 0);

                        if (ch.foregroundColor != cf ||
                                ch.backgroundColor != clipboard ||
                                (ch.rendition & ~RE_EXTENDED_CHAR) != (cr & ~RE_EXTENDED_CHAR) ||
                                !dirtyMask[x + len] ||
                                ch.isLineChar() != lineDraw ||
                                nextIsDoubleWidth != doubleWidth)
                            break;
                    }

                    const bool saveFixedFont = _fixedFont;
                    if (lineDraw)
                        _fixedFont = false;
                    if (doubleWidth)
                        _fixedFont = false;

                    updateLine = true;

                    _fixedFont = saveFixedFont;
                    x += len - 1;
                }
            }

        //both the top and bottom halves of double height _lines must always be redrawn
        //although both top and bottom halves contain the same characters, only
        //the top one is actually
        //drawn.
        if (_lineProperties.count() > y)
            updateLine |= (_lineProperties[y] & LINE_DOUBLEHEIGHT);

        // if the characters on the line are different in the old and the new _image
        // then this line must be repainted.
        if (updateLine) {
            dirtyLineCount++;

            // add the area occupied by this line to the region which needs to be
            // repainted
            QRect dirtyRect = QRect(_leftMargin + tLx ,
                                    _topMargin + tLy + _fontHeight * y ,
                                    _fontWidth * columnsToUpdate ,
                                    _fontHeight);

            dirtyRegion |= dirtyRect;
        }

        // replace the line of characters in the old _image with the
        // current line of the new _image
        memcpy((void*)currentLine, (const void*)newLine, columnsToUpdate * sizeof(Character));
    }

    // if the new _image is smaller than the previous _image, then ensure that the area
    // outside the new _image is cleared
    if (linesToUpdate < _usedLines) {
        dirtyRegion |= QRect(_leftMargin + tLx ,
                             _topMargin + tLy + _fontHeight * linesToUpdate ,
                             _fontWidth * this->_columns ,
                             _fontHeight * (_usedLines - linesToUpdate));
    }
    _usedLines = linesToUpdate;

    if (columnsToUpdate < _usedColumns) {
        dirtyRegion |= QRect(_leftMargin + tLx + columnsToUpdate * _fontWidth ,
                             _topMargin + tLy ,
                             _fontWidth * (_usedColumns - columnsToUpdate) ,
                             _fontHeight * this->_lines);
    }
    _usedColumns = columnsToUpdate;

    dirtyRegion |= _inputMethodData.previousPreeditRect;

    // update the parts of the display which have changed
    update(dirtyRegion);

    if (_allowBlinkingText && _hasTextBlinker && !_blinkTextTimer->isActive()) {
        _blinkTextTimer->start();
    }
    if (!_hasTextBlinker && _blinkTextTimer->isActive()) {
        _blinkTextTimer->stop();
        _textBlinking = false;
    }
    delete[] dirtyMask;

#if QT_VERSION >= 0x040800 // added in Qt 4.8.0
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::updateAccessibility(this, 0, QAccessible::TextUpdated);
    QAccessible::updateAccessibility(this, 0, QAccessible::TextCaretMoved);
#endif
#endif
}

void TerminalDisplay::showResizeNotification()
{
    if (_showTerminalSizeHint && isVisible()) {
        if (!_resizeWidget) {
            _resizeWidget = new QLabel(i18n("Size: XXX x XXX"), this);
            _resizeWidget->setMinimumWidth(_resizeWidget->fontMetrics().width(i18n("Size: XXX x XXX")));
            _resizeWidget->setMinimumHeight(_resizeWidget->sizeHint().height());
            _resizeWidget->setAlignment(Qt::AlignCenter);

            _resizeWidget->setStyleSheet("background-color:palette(window);border-style:solid;border-width:1px;border-color:palette(dark)");

            _resizeTimer = new QTimer(this);
            _resizeTimer->setInterval(SIZE_HINT_DURATION);
            _resizeTimer->setSingleShot(true);
            connect(_resizeTimer, SIGNAL(timeout()), _resizeWidget, SLOT(hide()));
        }
        QString sizeStr = i18n("Size: %1 x %2", _columns, _lines);
        _resizeWidget->setText(sizeStr);
        _resizeWidget->move((width() - _resizeWidget->width()) / 2,
                            (height() - _resizeWidget->height()) / 2 + 20);
        _resizeWidget->show();
        _resizeTimer->start();
    }
}

void TerminalDisplay::paintEvent(QPaintEvent* pe)
{
    QPainter paint(this);

    foreach(const QRect & rect, (pe->region() & contentsRect()).rects()) {
        drawBackground(paint, rect, palette().background().color(),
                       true /* use opacity setting */);
        drawContents(paint, rect);
    }
    drawInputMethodPreeditString(paint, preeditRect());
    paintFilters(paint);
}

void TerminalDisplay::printContent(QPainter& painter, bool friendly)
{
    // Reinitialize the font with the printers paint device so the font
    // measurement calculations will be done correctly
    QFont savedFont = getVTFont();
    QFont font(savedFont, painter.device());
    painter.setFont(font);
    setVTFont(font);

    QRect rect(0, 0, size().width(), size().height());

    _printerFriendly = friendly;
    if (!friendly) {
        drawBackground(painter, rect, getBackgroundColor(),
                       true /* use opacity setting */);
    }
    drawContents(painter, rect);
    _printerFriendly = false;
    setVTFont(savedFont);
}

QPoint TerminalDisplay::cursorPosition() const
{
    if (_screenWindow)
        return _screenWindow->cursorPosition();
    else
        return QPoint(0, 0);
}

FilterChain* TerminalDisplay::filterChain() const
{
    return _filterChain;
}

void TerminalDisplay::paintFilters(QPainter& painter)
{
    // get color of character under mouse and use it to draw
    // lines for filters
    QPoint cursorPos = mapFromGlobal(QCursor::pos());
    int cursorLine;
    int cursorColumn;
    const int scrollBarWidth = (_scrollbarLocation == Enum::ScrollBarLeft) ? _scrollBar->width() : 0;

    getCharacterPosition(cursorPos , cursorLine , cursorColumn);
    Character cursorCharacter = _image[loc(cursorColumn, cursorLine)];

    painter.setPen(QPen(cursorCharacter.foregroundColor.color(colorTable())));

    // iterate over hotspots identified by the display's currently active filters
    // and draw appropriate visuals to indicate the presence of the hotspot

    QList<Filter::HotSpot*> spots = _filterChain->hotSpots();
    foreach(Filter::HotSpot* spot, spots) {
        QRegion region;
        if (_underlineLinks && spot->type() == Filter::HotSpot::Link) {
            QRect r;
            if (spot->startLine() == spot->endLine()) {
                r.setCoords(spot->startColumn()*_fontWidth + 1 + scrollBarWidth,
                            spot->startLine()*_fontHeight + 1,
                            (spot->endColumn() - 1)*_fontWidth - 1 + scrollBarWidth,
                            (spot->endLine() + 1)*_fontHeight - 1);
                region |= r;
            } else {
                r.setCoords(spot->startColumn()*_fontWidth + 1 + scrollBarWidth,
                            spot->startLine()*_fontHeight + 1,
                            (_columns - 1)*_fontWidth - 1 + scrollBarWidth,
                            (spot->startLine() + 1)*_fontHeight - 1);
                region |= r;
                for (int line = spot->startLine() + 1 ; line < spot->endLine() ; line++) {
                    r.setCoords(0 * _fontWidth + 1 + scrollBarWidth,
                                line * _fontHeight + 1,
                                (_columns - 1)*_fontWidth - 1 + scrollBarWidth,
                                (line + 1)*_fontHeight - 1);
                    region |= r;
                }
                r.setCoords(0 * _fontWidth + 1 + scrollBarWidth,
                            spot->endLine()*_fontHeight + 1,
                            (spot->endColumn() - 1)*_fontWidth - 1 + scrollBarWidth,
                            (spot->endLine() + 1)*_fontHeight - 1);
                region |= r;
            }
        }

        for (int line = spot->startLine() ; line <= spot->endLine() ; line++) {
            int startColumn = 0;
            int endColumn = _columns - 1; // TODO use number of _columns which are actually
            // occupied on this line rather than the width of the
            // display in _columns

            // ignore whitespace at the end of the lines
            while (_image[loc(endColumn, line)].isSpace() && endColumn > 0)
                endColumn--;

            // increment here because the column which we want to set 'endColumn' to
            // is the first whitespace character at the end of the line
            endColumn++;

            if (line == spot->startLine())
                startColumn = spot->startColumn();
            if (line == spot->endLine())
                endColumn = spot->endColumn();

            // subtract one pixel from
            // the right and bottom so that
            // we do not overdraw adjacent
            // hotspots
            //
            // subtracting one pixel from all sides also prevents an edge case where
            // moving the mouse outside a link could still leave it underlined
            // because the check below for the position of the cursor
            // finds it on the border of the target area
            QRect r;
            r.setCoords(startColumn * _fontWidth + 1 + scrollBarWidth,
                        line * _fontHeight + 1,
                        endColumn * _fontWidth - 1 + scrollBarWidth,
                        (line + 1)*_fontHeight - 1);
            // Underline link hotspots
            if (_underlineLinks && spot->type() == Filter::HotSpot::Link) {
                QFontMetrics metrics(font());

                // find the baseline (which is the invisible line that the characters in the font sit on,
                // with some having tails dangling below)
                const int baseline = r.bottom() - metrics.descent();
                // find the position of the underline below that
                const int underlinePos = baseline + metrics.underlinePos();
                if (region.contains(mapFromGlobal(QCursor::pos()))) {
                    painter.drawLine(r.left() , underlinePos ,
                                     r.right() , underlinePos);
                }
                // Marker hotspots simply have a transparent rectanglular shape
                // drawn on top of them
            } else if (spot->type() == Filter::HotSpot::Marker) {
                //TODO - Do not use a hardcoded color for this
                painter.fillRect(r, QBrush(QColor(255, 0, 0, 120)));
            }
        }
    }
}
void TerminalDisplay::drawContents(QPainter& paint, const QRect& rect)
{
    const QPoint tL  = contentsRect().topLeft();
    const int    tLx = tL.x();
    const int    tLy = tL.y();

    const int lux = qMin(_usedColumns - 1, qMax(0, (rect.left()   - tLx - _leftMargin) / _fontWidth));
    const int luy = qMin(_usedLines - 1,  qMax(0, (rect.top()    - tLy - _topMargin) / _fontHeight));
    const int rlx = qMin(_usedColumns - 1, qMax(0, (rect.right()  - tLx - _leftMargin) / _fontWidth));
    const int rly = qMin(_usedLines - 1,  qMax(0, (rect.bottom() - tLy - _topMargin) / _fontHeight));

    const int numberOfColumns = _usedColumns;
    QString unistr;
    unistr.reserve(numberOfColumns);
    for (int y = luy; y <= rly; y++) {
        int x = lux;
        if (!_image[loc(lux, y)].character && x)
            x--; // Search for start of multi-column character
        for (; x <= rlx; x++) {
            int len = 1;
            int p = 0;

            // reset our buffer to the number of columns
            int bufferSize = numberOfColumns;
            unistr.resize(bufferSize);
            QChar *disstrU = unistr.data();

            // is this a single character or a sequence of characters ?
            if (_image[loc(x, y)].rendition & RE_EXTENDED_CHAR) {
                // sequence of characters
                ushort extendedCharLength = 0;
                const ushort* chars = ExtendedCharTable::instance.lookupExtendedChar(_image[loc(x, y)].character, extendedCharLength);
                if (chars) {
                    Q_ASSERT(extendedCharLength > 1);
                    bufferSize += extendedCharLength - 1;
                    unistr.resize(bufferSize);
                    disstrU = unistr.data();
                    for (int index = 0 ; index < extendedCharLength ; index++) {
                        Q_ASSERT(p < bufferSize);
                        disstrU[p++] = chars[index];
                    }
                }
            } else {
                // single character
                const quint16 c = _image[loc(x, y)].character;
                if (c) {
                    Q_ASSERT(p < bufferSize);
                    disstrU[p++] = c; //fontMap(c);
                }
            }

            const bool lineDraw = _image[loc(x, y)].isLineChar();
            const bool doubleWidth = (_image[ qMin(loc(x, y) + 1, _imageSize) ].character == 0);
            const CharacterColor currentForeground = _image[loc(x, y)].foregroundColor;
            const CharacterColor currentBackground = _image[loc(x, y)].backgroundColor;
            const quint8 currentRendition = _image[loc(x, y)].rendition;

            while (x + len <= rlx &&
                    _image[loc(x + len, y)].foregroundColor == currentForeground &&
                    _image[loc(x + len, y)].backgroundColor == currentBackground &&
                    (_image[loc(x + len, y)].rendition & ~RE_EXTENDED_CHAR) == (currentRendition & ~RE_EXTENDED_CHAR) &&
                    (_image[ qMin(loc(x + len, y) + 1, _imageSize) ].character == 0) == doubleWidth &&
                    _image[loc(x + len, y)].isLineChar() == lineDraw) {
                const quint16 c = _image[loc(x + len, y)].character;
                if (_image[loc(x + len, y)].rendition & RE_EXTENDED_CHAR) {
                    // sequence of characters
                    ushort extendedCharLength = 0;
                    const ushort* chars = ExtendedCharTable::instance.lookupExtendedChar(c, extendedCharLength);
                    if (chars) {
                        Q_ASSERT(extendedCharLength > 1);
                        bufferSize += extendedCharLength - 1;
                        unistr.resize(bufferSize);
                        disstrU = unistr.data();
                        for (int index = 0 ; index < extendedCharLength ; index++) {
                            Q_ASSERT(p < bufferSize);
                            disstrU[p++] = chars[index];
                        }
                    }
                } else {
                    // single character
                    if (c) {
                        Q_ASSERT(p < bufferSize);
                        disstrU[p++] = c; //fontMap(c);
                    }
                }

                if (doubleWidth) // assert((_image[loc(x+len,y)+1].character == 0)), see above if condition
                    len++; // Skip trailing part of multi-column character
                len++;
            }
            if ((x + len < _usedColumns) && (!_image[loc(x + len, y)].character))
                len++; // Adjust for trailing part of multi-column character

            const bool save__fixedFont = _fixedFont;
            if (lineDraw)
                _fixedFont = false;
            if (doubleWidth)
                _fixedFont = false;
            unistr.resize(p);

            // Create a text scaling matrix for double width and double height lines.
            QMatrix textScale;

            if (y < _lineProperties.size()) {
                if (_lineProperties[y] & LINE_DOUBLEWIDTH)
                    textScale.scale(2, 1);

                if (_lineProperties[y] & LINE_DOUBLEHEIGHT)
                    textScale.scale(1, 2);
            }

            //Apply text scaling matrix.
            paint.setWorldMatrix(textScale, true);

            //calculate the area in which the text will be drawn
            QRect textArea = QRect(_leftMargin + tLx + _fontWidth * x , _topMargin + tLy + _fontHeight * y , _fontWidth * len , _fontHeight);

            //move the calculated area to take account of scaling applied to the painter.
            //the position of the area from the origin (0,0) is scaled
            //by the opposite of whatever
            //transformation has been applied to the painter.  this ensures that
            //painting does actually start from textArea.topLeft()
            //(instead of textArea.topLeft() * painter-scale)
            textArea.moveTopLeft(textScale.inverted().map(textArea.topLeft()));

            //paint text fragment
            if (_printerFriendly) {
                drawPrinterFriendlyTextFragment(paint,
                                                textArea,
                                                unistr,
                                                &_image[loc(x, y)]);
            } else {
                drawTextFragment(paint,
                                 textArea,
                                 unistr,
                                 &_image[loc(x, y)]);
            }

            _fixedFont = save__fixedFont;

            //reset back to single-width, single-height _lines
            paint.setWorldMatrix(textScale.inverted(), true);

            if (y < _lineProperties.size() - 1) {
                //double-height _lines are represented by two adjacent _lines
                //containing the same characters
                //both _lines will have the LINE_DOUBLEHEIGHT attribute.
                //If the current line has the LINE_DOUBLEHEIGHT attribute,
                //we can therefore skip the next line
                if (_lineProperties[y] & LINE_DOUBLEHEIGHT)
                    y++;
            }

            x += len - 1;
        }
    }
}

QRect TerminalDisplay::imageToWidget(const QRect& imageArea) const
{
    QRect result;
    result.setLeft(_leftMargin + _fontWidth * imageArea.left());
    result.setTop(_topMargin + _fontHeight * imageArea.top());
    result.setWidth(_fontWidth * imageArea.width());
    result.setHeight(_fontHeight * imageArea.height());

    return result;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                          Blinking Text & Cursor                           */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::setBlinkingCursorEnabled(bool blink)
{
    _allowBlinkingCursor = blink;

    if (blink && !_blinkCursorTimer->isActive())
        _blinkCursorTimer->start();

    if (!blink && _blinkCursorTimer->isActive()) {
        _blinkCursorTimer->stop();
        if (_cursorBlinking) {
            // if cursor is blinking(hidden), blink it again to make it show
            blinkCursorEvent();
        }
        Q_ASSERT(_cursorBlinking == false);
    }
}

void TerminalDisplay::setBlinkingTextEnabled(bool blink)
{
    _allowBlinkingText = blink;

    if (blink && !_blinkTextTimer->isActive())
        _blinkTextTimer->start();

    if (!blink && _blinkTextTimer->isActive()) {
        _blinkTextTimer->stop();
        _textBlinking = false;
    }
}

void TerminalDisplay::focusOutEvent(QFocusEvent*)
{
    // trigger a repaint of the cursor so that it is both:
    //
    //   * visible (in case it was hidden during blinking)
    //   * drawn in a focused out state
    _cursorBlinking = false;
    updateCursor();

    // suppress further cursor blinking
    _blinkCursorTimer->stop();
    Q_ASSERT(_cursorBlinking == false);

    // if text is blinking (hidden), blink it again to make it shown
    if (_textBlinking)
        blinkTextEvent();

    // suppress further text blinking
    _blinkTextTimer->stop();
    Q_ASSERT(_textBlinking == false);
}

void TerminalDisplay::focusInEvent(QFocusEvent*)
{
    if (_allowBlinkingCursor)
        _blinkCursorTimer->start();

    updateCursor();

    if (_allowBlinkingText && _hasTextBlinker)
        _blinkTextTimer->start();
}

void TerminalDisplay::blinkTextEvent()
{
    Q_ASSERT(_allowBlinkingText);

    _textBlinking = !_textBlinking;

    // TODO: Optimize to only repaint the areas of the widget where there is
    // blinking text rather than repainting the whole widget.
    update();
}

void TerminalDisplay::blinkCursorEvent()
{
    Q_ASSERT(_allowBlinkingCursor);

    _cursorBlinking = !_cursorBlinking;
    updateCursor();
}

void TerminalDisplay::updateCursor()
{
    QRect cursorRect = imageToWidget(QRect(cursorPosition(), QSize(1, 1)));
    update(cursorRect);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                          Geometry & Resizing                              */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::resizeEvent(QResizeEvent*)
{
    updateImageSize();
}

void TerminalDisplay::propagateSize()
{
    if (_isFixedSize) {
        setSize(_columns, _lines);
        QWidget::setFixedSize(sizeHint());
        parentWidget()->adjustSize();
        parentWidget()->setFixedSize(parentWidget()->sizeHint());
        return;
    }
    if (_image)
        updateImageSize();
}

void TerminalDisplay::updateImageSize()
{
    Character* oldImage = _image;
    const int oldLines = _lines;
    const int oldColumns = _columns;

    makeImage();

    if (oldImage) {
        // copy the old image to reduce flicker
        int lines = qMin(oldLines, _lines);
        int columns = qMin(oldColumns, _columns);
        for (int line = 0; line < lines; line++) {
            memcpy((void*)&_image[_columns * line],
                   (void*)&oldImage[oldColumns * line],
                   columns * sizeof(Character));
        }
        delete[] oldImage;
    }

    if (_screenWindow)
        _screenWindow->setWindowLines(_lines);

    _resizing = (oldLines != _lines) || (oldColumns != _columns);

    if (_resizing) {
        showResizeNotification();
        emit changedContentSizeSignal(_contentHeight, _contentWidth); // expose resizeEvent
    }

    _resizing = false;
}

void TerminalDisplay::makeImage()
{
    _wallpaper->load();

    calcGeometry();

    // confirm that array will be of non-zero size, since the painting code
    // assumes a non-zero array length
    Q_ASSERT(_lines > 0 && _columns > 0);
    Q_ASSERT(_usedLines <= _lines && _usedColumns <= _columns);

    _imageSize = _lines * _columns;

    // We over-commit one character so that we can be more relaxed in dealing with
    // certain boundary conditions: _image[_imageSize] is a valid but unused position
    _image = new Character[_imageSize + 1];

    clearImage();
}

void TerminalDisplay::clearImage()
{
    for (int i = 0; i <= _imageSize; ++i)
        _image[i] = Screen::DefaultChar;
}

void TerminalDisplay::calcGeometry()
{
    _scrollBar->resize(_scrollBar->sizeHint().width(), contentsRect().height());
    switch (_scrollbarLocation) {
    case Enum::ScrollBarHidden :
        _leftMargin = DEFAULT_LEFT_MARGIN;
        _contentWidth = contentsRect().width() - 2 * DEFAULT_LEFT_MARGIN;
        break;
    case Enum::ScrollBarLeft :
        _leftMargin = DEFAULT_LEFT_MARGIN + _scrollBar->width();
        _contentWidth = contentsRect().width() - 2 * DEFAULT_LEFT_MARGIN - _scrollBar->width();
        _scrollBar->move(contentsRect().topLeft());
        break;
    case Enum::ScrollBarRight:
        _leftMargin = DEFAULT_LEFT_MARGIN;
        _contentWidth = contentsRect().width()  - 2 * DEFAULT_LEFT_MARGIN - _scrollBar->width();
        _scrollBar->move(contentsRect().topRight() - QPoint(_scrollBar->width() - 1, 0));
        break;
    }

    _topMargin = DEFAULT_TOP_MARGIN;
    _contentHeight = contentsRect().height() - 2 * DEFAULT_TOP_MARGIN + /* mysterious */ 1;

    if (!_isFixedSize) {
        // ensure that display is always at least one column wide
        _columns = qMax(1, _contentWidth / _fontWidth);
        _usedColumns = qMin(_usedColumns, _columns);

        // ensure that display is always at least one line high
        _lines = qMax(1, _contentHeight / _fontHeight);
        _usedLines = qMin(_usedLines, _lines);
    }
}

// calculate the needed size, this must be synced with calcGeometry()
void TerminalDisplay::setSize(int columns, int lines)
{
    const int scrollBarWidth = _scrollBar->isHidden() ? 0 : _scrollBar->sizeHint().width();
    const int horizontalMargin = 2 * DEFAULT_LEFT_MARGIN;
    const int verticalMargin = 2 * DEFAULT_TOP_MARGIN;

    QSize newSize = QSize(horizontalMargin + scrollBarWidth + (columns * _fontWidth)  ,
                          verticalMargin + (lines * _fontHeight));

    if (newSize != size()) {
        _size = newSize;
        updateGeometry();
    }
}

void TerminalDisplay::setFixedSize(int cols, int lins)
{
    _isFixedSize = true;

    //ensure that display is at least one line by one column in size
    _columns = qMax(1, cols);
    _lines = qMax(1, lins);
    _usedColumns = qMin(_usedColumns, _columns);
    _usedLines = qMin(_usedLines, _lines);

    if (_image) {
        delete[] _image;
        makeImage();
    }
    setSize(cols, lins);
    QWidget::setFixedSize(_size);
}

QSize TerminalDisplay::sizeHint() const
{
    return _size;
}

//showEvent and hideEvent are reimplemented here so that it appears to other classes that the
//display has been resized when the display is hidden or shown.
//
//TODO: Perhaps it would be better to have separate signals for show and hide instead of using
//the same signal as the one for a content size change
void TerminalDisplay::showEvent(QShowEvent*)
{
    emit changedContentSizeSignal(_contentHeight, _contentWidth);
}
void TerminalDisplay::hideEvent(QHideEvent*)
{
    emit changedContentSizeSignal(_contentHeight, _contentWidth);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Scrollbar                                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::setScrollBarPosition(Enum::ScrollBarPositionEnum position)
{
    if (_scrollbarLocation == position)
        return;

    if (position == Enum::ScrollBarHidden)
        _scrollBar->hide();
    else
        _scrollBar->show();

    _topMargin = _leftMargin = 1;
    _scrollbarLocation = position;

    propagateSize();
    update();
}

void TerminalDisplay::scrollBarPositionChanged(int)
{
    if (!_screenWindow)
        return;

    _screenWindow->scrollTo(_scrollBar->value());

    // if the thumb has been moved to the bottom of the _scrollBar then set
    // the display to automatically track new output,
    // that is, scroll down automatically
    // to how new _lines as they are added
    const bool atEndOfOutput = (_scrollBar->value() == _scrollBar->maximum());
    _screenWindow->setTrackOutput(atEndOfOutput);

    updateImage();
}

void TerminalDisplay::setScroll(int cursor, int slines)
{
    // update _scrollBar if the range or value has changed,
    // otherwise return
    //
    // setting the range or value of a _scrollBar will always trigger
    // a repaint, so it should be avoided if it is not necessary
    if (_scrollBar->minimum() == 0                 &&
            _scrollBar->maximum() == (slines - _lines) &&
            _scrollBar->value()   == cursor) {
        return;
    }

    disconnect(_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(scrollBarPositionChanged(int)));
    _scrollBar->setRange(0, slines - _lines);
    _scrollBar->setSingleStep(1);
    _scrollBar->setPageStep(_lines);
    _scrollBar->setValue(cursor);
    connect(_scrollBar, SIGNAL(valueChanged(int)), this, SLOT(scrollBarPositionChanged(int)));
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                  Mouse                                    */
/*                                                                           */
/* ------------------------------------------------------------------------- */
void TerminalDisplay::mousePressEvent(QMouseEvent* ev)
{
    if (_possibleTripleClick && (ev->button() == Qt::LeftButton)) {
        mouseTripleClickEvent(ev);
        return;
    }

    if (!contentsRect().contains(ev->pos())) return;

    if (!_screenWindow) return;

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn);
    QPoint pos = QPoint(charColumn, charLine);

    if (ev->button() == Qt::LeftButton) {
        // request the software keyboard, if any
        if (qApp->autoSipEnabled()) {
            QStyle::RequestSoftwareInputPanel behavior = QStyle::RequestSoftwareInputPanel(
                        style()->styleHint(QStyle::SH_RequestSoftwareInputPanel));
            if (hasFocus() || behavior == QStyle::RSIP_OnMouseClick) {
                QEvent event(QEvent::RequestSoftwareInputPanel);
                QApplication::sendEvent(this, &event);
            }
        }

        _lineSelectionMode = false;
        _wordSelectionMode = false;

        bool selected = false;
        // The user clicked inside selected text
        selected =  _screenWindow->isSelected(pos.x(), pos.y());

        // Drag only when the Control key is held
        if ((!_ctrlRequiredForDrag || ev->modifiers() & Qt::ControlModifier) && selected) {
            _dragInfo.state = diPending;
            _dragInfo.start = ev->pos();
        } else {
            // No reason to ever start a drag event
            _dragInfo.state = diNone;

            _preserveLineBreaks = !((ev->modifiers() & Qt::ControlModifier) && !(ev->modifiers() & Qt::AltModifier));
            _columnSelectionMode = (ev->modifiers() & Qt::AltModifier) && (ev->modifiers() & Qt::ControlModifier);

            if (_mouseMarks || (ev->modifiers() & Qt::ShiftModifier)) {
                _screenWindow->clearSelection();

                pos.ry() += _scrollBar->value();
                _iPntSel = _pntSel = pos;
                _actSel = 1; // left mouse button pressed but nothing selected yet.

            } else {
                emit mouseSignal(0, charColumn + 1, charLine + 1 + _scrollBar->value() - _scrollBar->maximum() , 0);
            }

            if (_underlineLinks && _openLinksByDirectClick) {
                Filter::HotSpot* spot = _filterChain->hotSpotAt(charLine, charColumn);
                if (spot && spot->type() == Filter::HotSpot::Link) {
                    QObject action;
                    action.setObjectName("open-action");
                    spot->activate(&action);
                }
            }
        }
    } else if (ev->button() == Qt::MidButton) {
        processMidButtonClick(ev);
    } else if (ev->button() == Qt::RightButton) {
        if (_mouseMarks || (ev->modifiers() & Qt::ShiftModifier))
            emit configureRequest(ev->pos());
        else
            emit mouseSignal(2, charColumn + 1, charLine + 1 + _scrollBar->value() - _scrollBar->maximum() , 0);
    }
}

QList<QAction*> TerminalDisplay::filterActions(const QPoint& position)
{
    int charLine, charColumn;
    getCharacterPosition(position, charLine, charColumn);

    Filter::HotSpot* spot = _filterChain->hotSpotAt(charLine, charColumn);

    return spot ? spot->actions() : QList<QAction*>();
}

void TerminalDisplay::mouseMoveEvent(QMouseEvent* ev)
{
    int charLine = 0;
    int charColumn = 0;
    getCharacterPosition(ev->pos(), charLine, charColumn);

    const int scrollBarWidth = (_scrollbarLocation == Enum::ScrollBarLeft) ? _scrollBar->width() : 0;

    // handle filters
    // change link hot-spot appearance on mouse-over
    Filter::HotSpot* spot = _filterChain->hotSpotAt(charLine, charColumn);
    if (spot && spot->type() == Filter::HotSpot::Link) {
        if (_underlineLinks) {
            QRegion previousHotspotArea = _mouseOverHotspotArea;
            _mouseOverHotspotArea = QRegion();
            QRect r;
            if (spot->startLine() == spot->endLine()) {
                r.setCoords(spot->startColumn()*_fontWidth + scrollBarWidth,
                            spot->startLine()*_fontHeight,
                            spot->endColumn()*_fontWidth + scrollBarWidth,
                            (spot->endLine() + 1)*_fontHeight - 1);
                _mouseOverHotspotArea |= r;
            } else {
                r.setCoords(spot->startColumn()*_fontWidth + scrollBarWidth,
                            spot->startLine()*_fontHeight,
                            _columns * _fontWidth - 1 + scrollBarWidth,
                            (spot->startLine() + 1)*_fontHeight);
                _mouseOverHotspotArea |= r;
                for (int line = spot->startLine() + 1 ; line < spot->endLine() ; line++) {
                    r.setCoords(0 * _fontWidth + scrollBarWidth,
                                line * _fontHeight,
                                _columns * _fontWidth + scrollBarWidth,
                                (line + 1)*_fontHeight);
                    _mouseOverHotspotArea |= r;
                }
                r.setCoords(0 * _fontWidth + scrollBarWidth,
                            spot->endLine()*_fontHeight,
                            spot->endColumn()*_fontWidth + scrollBarWidth,
                            (spot->endLine() + 1)*_fontHeight);
                _mouseOverHotspotArea |= r;
            }

            if (_openLinksByDirectClick && (cursor().shape() != Qt::PointingHandCursor))
                setCursor(Qt::PointingHandCursor);

            update(_mouseOverHotspotArea | previousHotspotArea);
        }
    } else if (!_mouseOverHotspotArea.isEmpty()) {
        if (_underlineLinks && _openLinksByDirectClick)
            setCursor(_mouseMarks ? Qt::IBeamCursor : Qt::ArrowCursor);

        update(_mouseOverHotspotArea);
        // set hotspot area to an invalid rectangle
        _mouseOverHotspotArea = QRegion();
    }

    // for auto-hiding the cursor, we need mouseTracking
    if (ev->buttons() == Qt::NoButton) return;

    // if the terminal is interested in mouse movements
    // then emit a mouse movement signal, unless the shift
    // key is being held down, which overrides this.
    if (!_mouseMarks && !(ev->modifiers() & Qt::ShiftModifier)) {
        int button = 3;
        if (ev->buttons() & Qt::LeftButton)
            button = 0;
        if (ev->buttons() & Qt::MidButton)
            button = 1;
        if (ev->buttons() & Qt::RightButton)
            button = 2;

        emit mouseSignal(button,
                         charColumn + 1,
                         charLine + 1 + _scrollBar->value() - _scrollBar->maximum(),
                         1);

        return;
    }

    if (_dragInfo.state == diPending) {
        // we had a mouse down, but haven't confirmed a drag yet
        // if the mouse has moved sufficiently, we will confirm

        const int distance = KGlobalSettings::dndEventDelay();
        if (ev->x() > _dragInfo.start.x() + distance || ev->x() < _dragInfo.start.x() - distance ||
                ev->y() > _dragInfo.start.y() + distance || ev->y() < _dragInfo.start.y() - distance) {
            // we've left the drag square, we can start a real drag operation now

            _screenWindow->clearSelection();
            doDrag();
        }
        return;
    } else if (_dragInfo.state == diDragging) {
        // this isn't technically needed because mouseMoveEvent is suppressed during
        // Qt drag operations, replaced by dragMoveEvent
        return;
    }

    if (_actSel == 0) return;

// don't extend selection while pasting
    if (ev->buttons() & Qt::MidButton) return;

    extendSelection(ev->pos());
}

void TerminalDisplay::extendSelection(const QPoint& position)
{
    if (!_screenWindow)
        return;

    //if ( !contentsRect().contains(ev->pos()) ) return;
    const QPoint tL  = contentsRect().topLeft();
    const int    tLx = tL.x();
    const int    tLy = tL.y();
    const int    scroll = _scrollBar->value();

    // we're in the process of moving the mouse with the left button pressed
    // the mouse cursor will kept caught within the bounds of the text in
    // this widget.

    int linesBeyondWidget = 0;

    QRect textBounds(tLx + _leftMargin,
                     tLy + _topMargin,
                     _usedColumns * _fontWidth - 1,
                     _usedLines * _fontHeight - 1);

    QPoint pos = position;

    // Adjust position within text area bounds.
    const QPoint oldpos = pos;

    pos.setX(qBound(textBounds.left(), pos.x(), textBounds.right()));
    pos.setY(qBound(textBounds.top(), pos.y(), textBounds.bottom()));

    if (oldpos.y() > textBounds.bottom()) {
        linesBeyondWidget = (oldpos.y() - textBounds.bottom()) / _fontHeight;
        _scrollBar->setValue(_scrollBar->value() + linesBeyondWidget + 1); // scrollforward
    }
    if (oldpos.y() < textBounds.top()) {
        linesBeyondWidget = (textBounds.top() - oldpos.y()) / _fontHeight;
        _scrollBar->setValue(_scrollBar->value() - linesBeyondWidget - 1); // history
    }

    int charColumn = 0;
    int charLine = 0;
    getCharacterPosition(pos, charLine, charColumn);

    QPoint here = QPoint(charColumn, charLine);
    QPoint ohere;
    QPoint _iPntSelCorr = _iPntSel;
    _iPntSelCorr.ry() -= _scrollBar->value();
    QPoint _pntSelCorr = _pntSel;
    _pntSelCorr.ry() -= _scrollBar->value();
    bool swapping = false;

    if (_wordSelectionMode) {
        // Extend to word boundaries
        int i;
        QChar selClass;

        const bool left_not_right = (here.y() < _iPntSelCorr.y() ||
                                     (here.y() == _iPntSelCorr.y() && here.x() < _iPntSelCorr.x()));
        const bool old_left_not_right = (_pntSelCorr.y() < _iPntSelCorr.y() ||
                                         (_pntSelCorr.y() == _iPntSelCorr.y() && _pntSelCorr.x() < _iPntSelCorr.x()));
        swapping = left_not_right != old_left_not_right;

        // Find left (left_not_right ? from here : from start)
        QPoint left = left_not_right ? here : _iPntSelCorr;
        i = loc(left.x(), left.y());
        if (i >= 0 && i <= _imageSize) {
            selClass = charClass(_image[i]);
            while (((left.x() > 0) || (left.y() > 0 && (_lineProperties[left.y() - 1] & LINE_WRAPPED)))
                    && charClass(_image[i - 1]) == selClass) {
                i--;
                if (left.x() > 0) left.rx()--;
                else {
                    left.rx() = _usedColumns - 1;
                    left.ry()--;
                }
            }
        }

        // Find left (left_not_right ? from start : from here)
        QPoint right = left_not_right ? _iPntSelCorr : here;
        i = loc(right.x(), right.y());
        if (i >= 0 && i <= _imageSize) {
            selClass = charClass(_image[i]);
            while (((right.x() < _usedColumns - 1) || (right.y() < _usedLines - 1 && (_lineProperties[right.y()] & LINE_WRAPPED)))
                    && charClass(_image[i + 1]) == selClass) {
                i++;
                if (right.x() < _usedColumns - 1) right.rx()++;
                else {
                    right.rx() = 0;
                    right.ry()++;
                }
            }
        }

        // Pick which is start (ohere) and which is extension (here)
        if (left_not_right) {
            here = left;
            ohere = right;
        } else {
            here = right;
            ohere = left;
        }
        ohere.rx()++;
    }

    if (_lineSelectionMode) {
        // Extend to complete line
        const bool above_not_below = (here.y() < _iPntSelCorr.y());

        QPoint above = above_not_below ? here : _iPntSelCorr;
        QPoint below = above_not_below ? _iPntSelCorr : here;

        while (above.y() > 0 && (_lineProperties[above.y() - 1] & LINE_WRAPPED))
            above.ry()--;
        while (below.y() < _usedLines - 1 && (_lineProperties[below.y()] & LINE_WRAPPED))
            below.ry()++;

        above.setX(0);
        below.setX(_usedColumns - 1);

        // Pick which is start (ohere) and which is extension (here)
        if (above_not_below) {
            here = above;
            ohere = below;
        } else {
            here = below;
            ohere = above;
        }

        const QPoint newSelBegin = QPoint(ohere.x(), ohere.y());
        swapping = !(_tripleSelBegin == newSelBegin);
        _tripleSelBegin = newSelBegin;

        ohere.rx()++;
    }

    int offset = 0;
    if (!_wordSelectionMode && !_lineSelectionMode) {
        QChar selClass;

        const bool left_not_right = (here.y() < _iPntSelCorr.y() ||
                                     (here.y() == _iPntSelCorr.y() && here.x() < _iPntSelCorr.x()));
        const bool old_left_not_right = (_pntSelCorr.y() < _iPntSelCorr.y() ||
                                         (_pntSelCorr.y() == _iPntSelCorr.y() && _pntSelCorr.x() < _iPntSelCorr.x()));
        swapping = left_not_right != old_left_not_right;

        // Find left (left_not_right ? from here : from start)
        const QPoint left = left_not_right ? here : _iPntSelCorr;

        // Find left (left_not_right ? from start : from here)
        QPoint right = left_not_right ? _iPntSelCorr : here;
        if (right.x() > 0 && !_columnSelectionMode) {
            int i = loc(right.x(), right.y());
            if (i >= 0 && i <= _imageSize) {
                selClass = charClass(_image[i - 1]);
                /* if (selClass == ' ')
                 {
                   while ( right.x() < _usedColumns-1 && charClass(_image[i+1].character) == selClass && (right.y()<_usedLines-1) &&
                                   !(_lineProperties[right.y()] & LINE_WRAPPED))
                   { i++; right.rx()++; }
                   if (right.x() < _usedColumns-1)
                     right = left_not_right ? _iPntSelCorr : here;
                   else
                     right.rx()++;  // will be balanced later because of offset=-1;
                 }*/
            }
        }

        // Pick which is start (ohere) and which is extension (here)
        if (left_not_right) {
            here = left;
            ohere = right;
            offset = 0;
        } else {
            here = right;
            ohere = left;
            offset = -1;
        }
    }

    if ((here == _pntSelCorr) && (scroll == _scrollBar->value())) return; // not moved

    if (here == ohere) return; // It's not left, it's not right.

    if (_actSel < 2 || swapping) {
        if (_columnSelectionMode && !_lineSelectionMode && !_wordSelectionMode) {
            _screenWindow->setSelectionStart(ohere.x() , ohere.y() , true);
        } else {
            _screenWindow->setSelectionStart(ohere.x() - 1 - offset , ohere.y() , false);
        }
    }

    _actSel = 2; // within selection
    _pntSel = here;
    _pntSel.ry() += _scrollBar->value();

    if (_columnSelectionMode && !_lineSelectionMode && !_wordSelectionMode) {
        _screenWindow->setSelectionEnd(here.x() , here.y());
    } else {
        _screenWindow->setSelectionEnd(here.x() + offset , here.y());
    }
}

void TerminalDisplay::mouseReleaseEvent(QMouseEvent* ev)
{
    if (!_screenWindow)
        return;

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn);

    if (ev->button() == Qt::LeftButton) {
        if (_dragInfo.state == diPending) {
            // We had a drag event pending but never confirmed.  Kill selection
            _screenWindow->clearSelection();
        } else {
            if (_actSel > 1) {
                copyToX11Selection();
            }

            _actSel = 0;

            //FIXME: emits a release event even if the mouse is
            //       outside the range. The procedure used in `mouseMoveEvent'
            //       applies here, too.

            if (!_mouseMarks && !(ev->modifiers() & Qt::ShiftModifier))
                emit mouseSignal(0,
                                 charColumn + 1,
                                 charLine + 1 + _scrollBar->value() - _scrollBar->maximum() , 2);
        }
        _dragInfo.state = diNone;
    }

    if (!_mouseMarks &&
            (ev->button() == Qt::RightButton || ev->button() == Qt::MidButton) &&
            !(ev->modifiers() & Qt::ShiftModifier)) {
        emit mouseSignal(ev->button() == Qt::MidButton ? 1 : 2,
                         charColumn + 1,
                         charLine + 1 + _scrollBar->value() - _scrollBar->maximum() ,
                         2);
    }
}

void TerminalDisplay::getCharacterPosition(const QPoint& widgetPoint, int& line, int& column) const
{
    column = (widgetPoint.x() + _fontWidth / 2 - contentsRect().left() - _leftMargin) / _fontWidth;
    line = (widgetPoint.y() - contentsRect().top() - _topMargin) / _fontHeight;

    if (line < 0)
        line = 0;
    if (column < 0)
        column = 0;

    if (line >= _usedLines)
        line = _usedLines - 1;

    // the column value returned can be equal to _usedColumns, which
    // is the position just after the last character displayed in a line.
    //
    // this is required so that the user can select characters in the right-most
    // column (or left-most for right-to-left input)
    if (column > _usedColumns)
        column = _usedColumns;
}

void TerminalDisplay::updateLineProperties()
{
    if (!_screenWindow)
        return;

    _lineProperties = _screenWindow->getLineProperties();
}

void TerminalDisplay::processMidButtonClick(QMouseEvent* ev)
{
    if (_mouseMarks || (ev->modifiers() & Qt::ShiftModifier)) {
        const bool appendEnter = ev->modifiers() & Qt::ControlModifier;

        if (_middleClickPasteMode == Enum::PasteFromX11Selection) {
            pasteFromX11Selection(appendEnter);
        } else if (_middleClickPasteMode == Enum::PasteFromClipboard) {
            pasteFromClipboard(appendEnter);
        } else {
            Q_ASSERT(false);
        }
    } else {
        int charLine = 0;
        int charColumn = 0;
        getCharacterPosition(ev->pos(), charLine, charColumn);

        emit mouseSignal(1, charColumn + 1, charLine + 1 + _scrollBar->value() - _scrollBar->maximum() , 0);
    }
}

void TerminalDisplay::mouseDoubleClickEvent(QMouseEvent* ev)
{
    // Yes, successive middle click can trigger this event
    if (ev->button() == Qt::MidButton) {
        processMidButtonClick(ev);
        return;
    }

    if (ev->button() != Qt::LeftButton) return;
    if (!_screenWindow) return;

    int charLine = 0;
    int charColumn = 0;

    getCharacterPosition(ev->pos(), charLine, charColumn);

    QPoint pos(charColumn, charLine);

    // pass on double click as two clicks.
    if (!_mouseMarks && !(ev->modifiers() & Qt::ShiftModifier)) {
        // Send just _ONE_ click event, since the first click of the double click
        // was already sent by the click handler
        emit mouseSignal(0,
                         pos.x() + 1,
                         pos.y() + 1 + _scrollBar->value() - _scrollBar->maximum(),
                         0);  // left button
        return;
    }

    _screenWindow->clearSelection();
    QPoint bgnSel = pos;
    QPoint endSel = pos;
    int i = loc(bgnSel.x(), bgnSel.y());
    _iPntSel = bgnSel;
    _iPntSel.ry() += _scrollBar->value();

    _wordSelectionMode = true;

    // find word boundaries...
    const QChar selClass = charClass(_image[i]);
    {
        // find the start of the word
        int x = bgnSel.x();
        while (((x > 0) || (bgnSel.y() > 0 && (_lineProperties[bgnSel.y() - 1] & LINE_WRAPPED)))
                && charClass(_image[i - 1]) == selClass) {
            i--;
            if (x > 0)
                x--;
            else {
                x = _usedColumns - 1;
                bgnSel.ry()--;
            }
        }

        bgnSel.setX(x);
        _screenWindow->setSelectionStart(bgnSel.x() , bgnSel.y() , false);

        // find the end of the word
        i = loc(endSel.x(), endSel.y());
        x = endSel.x();
        while (((x < _usedColumns - 1) || (endSel.y() < _usedLines - 1 && (_lineProperties[endSel.y()] & LINE_WRAPPED)))
                && charClass(_image[i + 1]) == selClass) {
            i++;
            if (x < _usedColumns - 1)
                x++;
            else {
                x = 0;
                endSel.ry()++;
            }
        }

        endSel.setX(x);

        // In word selection mode don't select @ (64) if at end of word.
        if (((_image[i].rendition & RE_EXTENDED_CHAR) == 0) &&
                (QChar(_image[i].character) == '@') &&
                ((endSel.x() - bgnSel.x()) > 0)) {
            endSel.setX(x - 1);
        }

        _actSel = 2; // within selection

        _screenWindow->setSelectionEnd(endSel.x() , endSel.y());

        copyToX11Selection();
    }

    _possibleTripleClick = true;

    QTimer::singleShot(QApplication::doubleClickInterval(), this,
                       SLOT(tripleClickTimeout()));
}

void TerminalDisplay::wheelEvent(QWheelEvent* ev)
{
    // Only vertical scrolling is supported
    if (ev->orientation() != Qt::Vertical)
        return;

    const int modifiers = ev->modifiers();
    const int delta = ev->delta();

    // ctrl+<wheel> for zomming, like in konqueror and firefox
    if (modifiers & Qt::ControlModifier) {
        if (delta > 0) {
            // wheel-up for increasing font size
            increaseFontSize();
        } else {
            // wheel-down for decreasing font size
            decreaseFontSize();
        }

        return;
    }

    // if the terminal program is not interested with mouse events:
    //  * send the event to the scrollbar if the slider has room to move
    //  * otherwise, send simulated up / down key presses to the terminal program
    //    for the benefit of programs such as 'less'
    if (_mouseMarks) {
        const bool canScroll = _scrollBar->maximum() > 0;
        if (canScroll) {
            _scrollBar->event(ev);
        } else {
            // assume that each Up / Down key event will cause the terminal application
            // to scroll by one line.
            //
            // to get a reasonable scrolling speed, scroll by one line for every 5 degrees
            // of mouse wheel rotation.  Mouse wheels typically move in steps of 15 degrees,
            // giving a scroll of 3 lines
            const int keyCode = delta > 0 ? Qt::Key_Up : Qt::Key_Down;
            QKeyEvent keyEvent(QEvent::KeyPress, keyCode, Qt::NoModifier);

            // QWheelEvent::delta() gives rotation in eighths of a degree
            const int degrees = delta / 8;
            const int lines = abs(degrees) / 5;

            for (int i = 0; i < lines; i++)
                emit keyPressedSignal(&keyEvent);
        }
    } else {
        // terminal program wants notification of mouse activity

        int charLine;
        int charColumn;
        getCharacterPosition(ev->pos() , charLine , charColumn);

        emit mouseSignal(delta > 0 ? 4 : 5,
                         charColumn + 1,
                         charLine + 1 + _scrollBar->value() - _scrollBar->maximum() ,
                         0);
    }
}

void TerminalDisplay::tripleClickTimeout()
{
    _possibleTripleClick = false;
}

void TerminalDisplay::mouseTripleClickEvent(QMouseEvent* ev)
{
    if (!_screenWindow) return;

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn);
    _iPntSel = QPoint(charColumn, charLine);

    _screenWindow->clearSelection();

    _lineSelectionMode = true;
    _wordSelectionMode = false;

    _actSel = 2; // within selection

    while (_iPntSel.y() > 0 && (_lineProperties[_iPntSel.y() - 1] & LINE_WRAPPED))
        _iPntSel.ry()--;

    if (_tripleClickMode == Enum::SelectForwardsFromCursor) {
        // find word boundary start
        int i = loc(_iPntSel.x(), _iPntSel.y());
        const QChar selClass = charClass(_image[i]);
        int x = _iPntSel.x();

        while (((x > 0) ||
                (_iPntSel.y() > 0 && (_lineProperties[_iPntSel.y() - 1] & LINE_WRAPPED))
               )
                && charClass(_image[i - 1]) == selClass) {
            i--;
            if (x > 0)
                x--;
            else {
                x = _columns - 1;
                _iPntSel.ry()--;
            }
        }

        _screenWindow->setSelectionStart(x , _iPntSel.y() , false);
        _tripleSelBegin = QPoint(x, _iPntSel.y());
    } else if (_tripleClickMode == Enum::SelectWholeLine) {
        _screenWindow->setSelectionStart(0 , _iPntSel.y() , false);
        _tripleSelBegin = QPoint(0, _iPntSel.y());
    }

    while (_iPntSel.y() < _lines - 1 && (_lineProperties[_iPntSel.y()] & LINE_WRAPPED))
        _iPntSel.ry()++;

    _screenWindow->setSelectionEnd(_columns - 1 , _iPntSel.y());

    copyToX11Selection();

    _iPntSel.ry() += _scrollBar->value();
}

bool TerminalDisplay::focusNextPrevChild(bool next)
{
    // for 'Tab', always disable focus switching among widgets
    // for 'Shift+Tab', leave the decision to higher level
    if (next)
        return false;
    else
        return QWidget::focusNextPrevChild(next);
}

QChar TerminalDisplay::charClass(const Character& ch) const
{
    if (ch.rendition & RE_EXTENDED_CHAR) {
        ushort extendedCharLength = 0;
        const ushort* chars = ExtendedCharTable::instance.lookupExtendedChar(ch.character, extendedCharLength);
        if (chars && extendedCharLength > 0) {
            const QString s = QString::fromUtf16(chars, extendedCharLength);
            if (_wordCharacters.contains(s, Qt::CaseInsensitive))
                return 'a';
            bool allLetterOrNumber = true;
            for (int i = 0; allLetterOrNumber && i < s.size(); ++i)
                allLetterOrNumber = s.at(i).isLetterOrNumber();
            return allLetterOrNumber ? 'a' : s.at(0);
        }
        return 0;
    } else {
        const QChar qch(ch.character);
        if (qch.isSpace()) return ' ';

        if (qch.isLetterOrNumber() || _wordCharacters.contains(qch, Qt::CaseInsensitive))
            return 'a';

        return qch;
    }
}

void TerminalDisplay::setWordCharacters(const QString& wc)
{
    _wordCharacters = wc;
}

// FIXME: the actual value of _mouseMarks is the opposite of its semantic.
// When using programs not interested with mouse(shell, less), it is true.
// When using programs interested with mouse(vim,mc), it is false.
void TerminalDisplay::setUsesMouse(bool on)
{
    _mouseMarks = on;
    setCursor(_mouseMarks ? Qt::IBeamCursor : Qt::ArrowCursor);
}
bool TerminalDisplay::usesMouse() const
{
    return _mouseMarks;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               Clipboard                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::doPaste(QString text, bool appendReturn)
{
    if (!_screenWindow)
        return;

    if (appendReturn)
        text.append("\r");

    if (!text.isEmpty()) {
        text.replace('\n', '\r');
        // perform paste by simulating keypress events
        QKeyEvent e(QEvent::KeyPress, 0, Qt::NoModifier, text);
        emit keyPressedSignal(&e);
    }
}

void TerminalDisplay::setAutoCopySelectedText(bool enabled)
{
    _autoCopySelectedText = enabled;
}

void TerminalDisplay::setMiddleClickPasteMode(Enum::MiddleClickPasteModeEnum mode)
{
    _middleClickPasteMode = mode;
}

void TerminalDisplay::copyToX11Selection()
{
    if (!_screenWindow)
        return;

    QString text = _screenWindow->selectedText(_preserveLineBreaks, _trimTrailingSpaces);
    if (text.isEmpty())
        return;

    QApplication::clipboard()->setText(text, QClipboard::Selection);

    if (_autoCopySelectedText)
        QApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

void TerminalDisplay::copyToClipboard()
{
    if (!_screenWindow)
        return;

    QString text = _screenWindow->selectedText(_preserveLineBreaks, _trimTrailingSpaces);
    if (text.isEmpty())
        return;

    QApplication::clipboard()->setText(text, QClipboard::Clipboard);
}

void TerminalDisplay::pasteFromClipboard(bool appendEnter)
{
    QString text = QApplication::clipboard()->text(QClipboard::Clipboard);
    doPaste(text, appendEnter);
}

void TerminalDisplay::pasteFromX11Selection(bool appendEnter)
{
    QString text = QApplication::clipboard()->text(QClipboard::Selection);
    doPaste(text, appendEnter);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Input Method                               */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::inputMethodEvent(QInputMethodEvent* event)
{
    if (!event->commitString().isEmpty()) {
        QKeyEvent keyEvent(QEvent::KeyPress, 0, Qt::NoModifier, event->commitString());
        emit keyPressedSignal(&keyEvent);
    }

    _inputMethodData.preeditString = event->preeditString();
    update(preeditRect() | _inputMethodData.previousPreeditRect);

    event->accept();
}

QVariant TerminalDisplay::inputMethodQuery(Qt::InputMethodQuery query) const
{
    const QPoint cursorPos = cursorPosition();
    switch (query) {
    case Qt::ImMicroFocus:
        return imageToWidget(QRect(cursorPos.x(), cursorPos.y(), 1, 1));
        break;
    case Qt::ImFont:
        return font();
        break;
    case Qt::ImCursorPosition:
        // return the cursor position within the current line
        return cursorPos.x();
        break;
    case Qt::ImSurroundingText: {
        // return the text from the current line
        QString lineText;
        QTextStream stream(&lineText);
        PlainTextDecoder decoder;
        decoder.begin(&stream);
        decoder.decodeLine(&_image[loc(0, cursorPos.y())], _usedColumns, _lineProperties[cursorPos.y()]);
        decoder.end();
        return lineText;
    }
    break;
    case Qt::ImCurrentSelection:
        return QString();
        break;
    default:
        break;
    }

    return QVariant();
}

QRect TerminalDisplay::preeditRect() const
{
    const int preeditLength = string_width(_inputMethodData.preeditString);

    if (preeditLength == 0)
        return QRect();

    return QRect(_leftMargin + _fontWidth * cursorPosition().x(),
                 _topMargin + _fontHeight * cursorPosition().y(),
                 _fontWidth * preeditLength,
                 _fontHeight);
}

void TerminalDisplay::drawInputMethodPreeditString(QPainter& painter , const QRect& rect)
{
    if (_inputMethodData.preeditString.isEmpty())
        return;

    const QPoint cursorPos = cursorPosition();

    bool invertColors = false;
    const QColor background = _colorTable[DEFAULT_BACK_COLOR].color;
    const QColor foreground = _colorTable[DEFAULT_FORE_COLOR].color;
    const Character* style = &_image[loc(cursorPos.x(), cursorPos.y())];

    drawBackground(painter, rect, background, true);
    drawCursor(painter, rect, foreground, background, invertColors);
    drawCharacters(painter, rect, _inputMethodData.preeditString, style, invertColors);

    _inputMethodData.previousPreeditRect = rect;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Keyboard                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::setFlowControlWarningEnabled(bool enable)
{
    _flowControlWarningEnabled = enable;

    // if the dialog is currently visible and the flow control warning has
    // been disabled then hide the dialog
    if (!enable)
        outputSuspended(false);
}

void TerminalDisplay::outputSuspended(bool suspended)
{
    //create the label when this function is first called
    if (!_outputSuspendedLabel) {
        //This label includes a link to an English language website
        //describing the 'flow control' (Xon/Xoff) feature found in almost
        //all terminal emulators.
        //If there isn't a suitable article available in the target language the link
        //can simply be removed.
        _outputSuspendedLabel = new QLabel(i18n("<qt>Output has been "
                                                "<a href=\"http://en.wikipedia.org/wiki/Flow_control\">suspended</a>"
                                                " by pressing Ctrl+S."
                                                "  Press <b>Ctrl+Q</b> to resume.</qt>"),
                                           this);

        QPalette palette(_outputSuspendedLabel->palette());
        KColorScheme::adjustBackground(palette, KColorScheme::NeutralBackground);
        _outputSuspendedLabel->setPalette(palette);
        _outputSuspendedLabel->setAutoFillBackground(true);
        _outputSuspendedLabel->setBackgroundRole(QPalette::Base);
        _outputSuspendedLabel->setFont(KGlobalSettings::generalFont());
        _outputSuspendedLabel->setContentsMargins(5, 5, 5, 5);

        //enable activation of "Xon/Xoff" link in label
        _outputSuspendedLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse |
                Qt::LinksAccessibleByKeyboard);
        _outputSuspendedLabel->setOpenExternalLinks(true);
        _outputSuspendedLabel->setVisible(false);

        _gridLayout->addWidget(_outputSuspendedLabel);
        _gridLayout->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding,
                                             QSizePolicy::Expanding),
                             1, 0);
    }

    _outputSuspendedLabel->setVisible(suspended);
}

void TerminalDisplay::scrollScreenWindow(enum ScreenWindow::RelativeScrollMode mode, int amount)
{
    _screenWindow->scrollBy(mode, amount);
    _screenWindow->setTrackOutput(_screenWindow->atEndOfOutput());
    updateLineProperties();
    updateImage();
}

void TerminalDisplay::keyPressEvent(QKeyEvent* event)
{
    _screenWindow->screen()->setCurrentTerminalDisplay(this);

    _actSel = 0; // Key stroke implies a screen update, so TerminalDisplay won't
    // know where the current selection is.

    if (_allowBlinkingCursor) {
        _blinkCursorTimer->start();
        if (_cursorBlinking) {
            // if cursor is blinking(hidden), blink it again to show it
            blinkCursorEvent();
        }
        Q_ASSERT(_cursorBlinking == false);
    }

    emit keyPressedSignal(event);

#if QT_VERSION >= 0x040800 // added in Qt 4.8.0
#ifndef QT_NO_ACCESSIBILITY
    QAccessible::updateAccessibility(this, 0, QAccessible::TextCaretMoved);
#endif
#endif

    event->accept();
}

bool TerminalDisplay::handleShortcutOverrideEvent(QKeyEvent* keyEvent)
{
    const int modifiers = keyEvent->modifiers();

    //  When a possible shortcut combination is pressed,
    //  emit the overrideShortcutCheck() signal to allow the host
    //  to decide whether the terminal should override it or not.
    if (modifiers != Qt::NoModifier) {
        int modifierCount = 0;
        unsigned int currentModifier = Qt::ShiftModifier;

        while (currentModifier <= Qt::KeypadModifier) {
            if (modifiers & currentModifier)
                modifierCount++;
            currentModifier <<= 1;
        }
        if (modifierCount < 2) {
            bool override = false;
            emit overrideShortcutCheck(keyEvent, override);
            if (override) {
                keyEvent->accept();
                return true;
            }
        }
    }

    // Override any of the following shortcuts because
    // they are needed by the terminal
    int keyCode = keyEvent->key() | modifiers;
    switch (keyCode) {
        // list is taken from the QLineEdit::event() code
    case Qt::Key_Tab:
    case Qt::Key_Delete:
    case Qt::Key_Home:
    case Qt::Key_End:
    case Qt::Key_Backspace:
    case Qt::Key_Left:
    case Qt::Key_Right:
    case Qt::Key_Slash:
    case Qt::Key_Period:
    case Qt::Key_Space:
        keyEvent->accept();
        return true;
    }
    return false;
}

bool TerminalDisplay::event(QEvent* event)
{
    bool eventHandled = false;
    switch (event->type()) {
    case QEvent::ShortcutOverride:
        eventHandled = handleShortcutOverrideEvent(static_cast<QKeyEvent*>(event));
        break;
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
        _scrollBar->setPalette(QApplication::palette());
        break;
    default:
        break;
    }
    return eventHandled ? true : QWidget::event(event);
}

void TerminalDisplay::contextMenuEvent(QContextMenuEvent* event)
{
    // the logic for the mouse case is within MousePressEvent()
    if (event->reason() != QContextMenuEvent::Mouse) {
        emit configureRequest(mapFromGlobal(QCursor::pos()));
    }
}

/* --------------------------------------------------------------------- */
/*                                                                       */
/*                                  Bell                                 */
/*                                                                       */
/* --------------------------------------------------------------------- */

void TerminalDisplay::setBellMode(int mode)
{
    _bellMode = mode;
}

int TerminalDisplay::bellMode() const
{
    return _bellMode;
}

void TerminalDisplay::unmaskBell()
{
    _bellMasked = false;
}

void TerminalDisplay::bell(const QString& message)
{
    if (_bellMasked)
        return;

    switch (_bellMode) {
    case Enum::SystemBeepBell:
        KNotification::beep();
        break;
    case Enum::NotifyBell:
        // STABLE API:
        //     Please note that these event names, "BellVisible" and "BellInvisible",
        //     should not change and should be kept stable, because other applications
        //     that use this code via KPart rely on these names for notifications.
        KNotification::event(hasFocus() ? "BellVisible" : "BellInvisible",
                             message, QPixmap(), this);
        break;
    case Enum::VisualBell:
        visualBell();
        break;
    default:
        break;
    }

    // limit the rate at which bells can occur.
    // ...mainly for sound effects where rapid bells in sequence
    // produce a horrible noise.
    _bellMasked = true;
    QTimer::singleShot(500, this, SLOT(unmaskBell()));
}

void TerminalDisplay::visualBell()
{
    swapFGBGColors();
    QTimer::singleShot(200, this, SLOT(swapFGBGColors()));
}

void TerminalDisplay::swapFGBGColors()
{
    // swap the default foreground & backround color
    ColorEntry color = _colorTable[DEFAULT_BACK_COLOR];
    _colorTable[DEFAULT_BACK_COLOR] = _colorTable[DEFAULT_FORE_COLOR];
    _colorTable[DEFAULT_FORE_COLOR] = color;

    update();
}

/* --------------------------------------------------------------------- */
/*                                                                       */
/* Drag & Drop                                                           */
/*                                                                       */
/* --------------------------------------------------------------------- */

void TerminalDisplay::dragEnterEvent(QDragEnterEvent* event)
{
    // text/plain alone is enough for KDE-apps
    // text/uri-list is for supporting some non-KDE apps, such as thunar
    //   and pcmanfm
    // That also applies in dropEvent()
    if (event->mimeData()->hasFormat("text/plain") ||
            event->mimeData()->hasFormat("text/uri-list")) {
        event->acceptProposedAction();
    }
}

void TerminalDisplay::dropEvent(QDropEvent* event)
{
    KUrl::List urls = KUrl::List::fromMimeData(event->mimeData());

    QString dropText;
    if (!urls.isEmpty()) {
        for (int i = 0 ; i < urls.count() ; i++) {
            KUrl url = KIO::NetAccess::mostLocalUrl(urls[i] , 0);
            QString urlText;

            if (url.isLocalFile())
                urlText = url.path();
            else
                urlText = url.url();

            // in future it may be useful to be able to insert file names with drag-and-drop
            // without quoting them (this only affects paths with spaces in)
            urlText = KShell::quoteArg(urlText);

            dropText += urlText;

            // Each filename(including the last) should be followed by one space.
            dropText += ' ';
        }

        // If our target is local we will open a popup - otherwise the fallback kicks
        // in and the URLs will simply be pasted as text.
        if (_sessionController && _sessionController->url().isLocalFile()) {
            // A standard popup with Copy, Move and Link as options -
            // plus an additional Paste option.

            QAction* pasteAction = new QAction(i18n("&Paste Location"), this);
            pasteAction->setData(dropText);
            connect(pasteAction, SIGNAL(triggered()), this, SLOT(dropMenuPasteActionTriggered()));

            QList<QAction*> additionalActions;
            additionalActions.append(pasteAction);

            if (urls.count() == 1) {
                const KUrl url = KIO::NetAccess::mostLocalUrl(urls[0] , 0);

                if (url.isLocalFile()) {
                    const QFileInfo fileInfo(url.path());

                    if (fileInfo.isDir()) {
                        QAction* cdAction = new QAction(i18n("Change &Directory To"), this);
                        dropText = QLatin1String(" cd ") + dropText + QChar('\n');
                        cdAction->setData(dropText);
                        connect(cdAction, SIGNAL(triggered()), this, SLOT(dropMenuCdActionTriggered()));
                        additionalActions.append(cdAction);
                    }
                }
            }

            KUrl target(_sessionController->currentDir());

            KonqOperations::doDrop(KFileItem(), target, event, this, additionalActions);

            return;
        }

    } else {
        dropText = event->mimeData()->text();
    }

    if (event->mimeData()->hasFormat("text/plain") ||
            event->mimeData()->hasFormat("text/uri-list")) {
        emit sendStringToEmu(dropText.toLocal8Bit());
    }
}

void TerminalDisplay::dropMenuPasteActionTriggered()
{
    if (sender()) {
        const QAction* action = qobject_cast<const QAction*>(sender());
        if (action) {
            emit sendStringToEmu(action->data().toString().toLocal8Bit());
        }
    }
}

void TerminalDisplay::dropMenuCdActionTriggered()
{
    if (sender()) {
        const QAction* action = qobject_cast<const QAction*>(sender());
        if (action) {
            emit sendStringToEmu(action->data().toString().toLocal8Bit());
        }
    }
}

void TerminalDisplay::doDrag()
{
    _dragInfo.state = diDragging;
    _dragInfo.dragObject = new QDrag(this);
    QMimeData* mimeData = new QMimeData;
    mimeData->setText(QApplication::clipboard()->text(QClipboard::Selection));
    _dragInfo.dragObject->setMimeData(mimeData);
    _dragInfo.dragObject->exec(Qt::CopyAction);
}

void TerminalDisplay::setSessionController(SessionController* controller)
{
    _sessionController = controller;
}

AutoScrollHandler::AutoScrollHandler(QWidget* parent)
    : QObject(parent)
    , _timerId(0)
{
    parent->installEventFilter(this);
}
void AutoScrollHandler::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != _timerId)
        return;

    QMouseEvent mouseEvent(QEvent::MouseMove,
                           widget()->mapFromGlobal(QCursor::pos()),
                           Qt::NoButton,
                           Qt::LeftButton,
                           Qt::NoModifier);

    QApplication::sendEvent(widget(), &mouseEvent);
}
bool AutoScrollHandler::eventFilter(QObject* watched, QEvent* event)
{
    Q_ASSERT(watched == parent());
    Q_UNUSED(watched);

    QMouseEvent* mouseEvent = (QMouseEvent*)event;
    switch (event->type()) {
    case QEvent::MouseMove: {
        bool mouseInWidget = widget()->rect().contains(mouseEvent->pos());
        if (mouseInWidget) {
            if (_timerId)
                killTimer(_timerId);

            _timerId = 0;
        } else {
            if (!_timerId && (mouseEvent->buttons() & Qt::LeftButton))
                _timerId = startTimer(100);
        }

        break;
    }
    case QEvent::MouseButtonRelease: {
        if (_timerId && (mouseEvent->buttons() & ~Qt::LeftButton)) {
            killTimer(_timerId);
            _timerId = 0;
        }
        break;
    }
    default:
        break;
    };

    return false;
}

#include "TerminalDisplay.moc"
