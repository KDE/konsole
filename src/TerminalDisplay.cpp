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
#include "KonsoleSettings.h"

// Config
#include "config-konsole.h"

// Qt
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QEvent>
#include <QFileInfo>
#include <QVBoxLayout>
#include <QAction>
#include <QLabel>
#include <QMimeData>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QStyle>
#include <QTimer>
#include <QDrag>
#include <QDesktopServices>
#include <QAccessible>

// KDE
#include <KShell>
#include <KColorScheme>
#include <KCursor>
#include <KLocalizedString>
#include <KNotification>
#include <KIO/DropJob>
#include <KJobWidgets>
#include <KMessageBox>
#include <KMessageWidget>
#include <KIO/StatJob>

// Konsole
#include "Filter.h"
#include "konsoledebug.h"
#include "TerminalCharacterDecoder.h"
#include "Screen.h"
#include "SessionController.h"
#include "ExtendedCharTable.h"
#include "TerminalDisplayAccessible.h"
#include "SessionManager.h"
#include "Session.h"
#include "WindowSystemInfo.h"
#include "IncrementalSearchBar.h"
#include "Profile.h"
#include "ViewManager.h" // for colorSchemeForProfile. // TODO: Rewrite this.
#include "LineBlockCharacters.h"

using namespace Konsole;

#define REPCHAR   "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "abcdefgjijklmnopqrstuvwxyz" \
    "0123456789./+@"

// we use this to force QPainter to display text in LTR mode
// more information can be found in: https://unicode.org/reports/tr9/
const QChar LTR_OVERRIDE_CHAR(0x202D);

inline int TerminalDisplay::loc(int x, int y) const {
    Q_ASSERT(y >= 0 && y < _lines);
    Q_ASSERT(x >= 0 && x < _columns);
    x = qBound(0, x, _columns - 1);
    y = qBound(0, y, _lines - 1);

    return y * _columns + x;
}

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
    if (!_screenWindow.isNull()) {
        disconnect(_screenWindow , nullptr , this , nullptr);
    }

    _screenWindow = window;

    if (!_screenWindow.isNull()) {
        connect(_screenWindow.data() , &Konsole::ScreenWindow::outputChanged , this , &Konsole::TerminalDisplay::updateLineProperties);
        connect(_screenWindow.data() , &Konsole::ScreenWindow::outputChanged , this , &Konsole::TerminalDisplay::updateImage);
        connect(_screenWindow.data() , &Konsole::ScreenWindow::currentResultLineChanged , this , &Konsole::TerminalDisplay::updateImage);
        connect(_screenWindow.data(), &Konsole::ScreenWindow::outputChanged, this, [this]() {
            _filterUpdateRequired = true;
        });
        connect(_screenWindow.data(), &Konsole::ScreenWindow::screenAboutToChange, this, [this]() {
            _iPntSel = QPoint();
            _pntSel = QPoint();
            _tripleSelBegin = QPoint();
        });
        connect(_screenWindow.data(), &Konsole::ScreenWindow::scrolled, this, [this]() {
            _filterUpdateRequired = true;
        });
        connect(_screenWindow.data(), &Konsole::ScreenWindow::outputChanged, this, []() {
            QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
        });
        _screenWindow->setWindowLines(_lines);
    }
}

const ColorEntry* TerminalDisplay::colorTable() const
{
    return _colorTable;
}

void TerminalDisplay::onColorsChanged()
{
    // Mostly just fix the scrollbar
    // this is a workaround to add some readability to old themes like Fusion
    // changing the light value for button a bit makes themes like fusion, windows and oxygen way more readable and pleasing

    QPalette p = QApplication::palette();

    QColor buttonTextColor = _colorTable[DEFAULT_FORE_COLOR];
    QColor backgroundColor = _colorTable[DEFAULT_BACK_COLOR];
    backgroundColor.setAlphaF(_opacity);

    QColor buttonColor = backgroundColor.toHsv();
    if (buttonColor.valueF() < 0.5) {
        buttonColor = buttonColor.lighter();
    } else {
        buttonColor = buttonColor.darker();
    }
    p.setColor(QPalette::Button, buttonColor);
    p.setColor(QPalette::Window, backgroundColor);
    p.setColor(QPalette::Base, backgroundColor);
    p.setColor(QPalette::WindowText, buttonTextColor);
    p.setColor(QPalette::ButtonText, buttonTextColor);

    setPalette(p);

    _scrollBar->setPalette(p);

    update();
}

void TerminalDisplay::setBackgroundColor(const QColor& color)
{
    _colorTable[DEFAULT_BACK_COLOR] = color;

    onColorsChanged();
}

QColor TerminalDisplay::getBackgroundColor() const
{
    return _colorTable[DEFAULT_BACK_COLOR];
}

void TerminalDisplay::setForegroundColor(const QColor& color)
{
    _colorTable[DEFAULT_FORE_COLOR] = color;

    onColorsChanged();
}

QColor TerminalDisplay::getForegroundColor() const
{
    return _colorTable[DEFAULT_FORE_COLOR];
}

void TerminalDisplay::setColorTable(const ColorEntry table[])
{
    for (int i = 0; i < TABLE_COLORS; i++) {
        _colorTable[i] = table[i];
    }

    setBackgroundColor(_colorTable[DEFAULT_BACK_COLOR]);

    onColorsChanged();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                   Font                                    */
/*                                                                           */
/* ------------------------------------------------------------------------- */

static inline bool isLineCharString(const QString& string)
{
    if (string.length() == 0) {
        return false;
    }

    return LineBlockCharacters::canDraw(string.at(0).unicode());
}

void TerminalDisplay::fontChange(const QFont&)
{
    QFontMetrics fm(font());
    _fontHeight = fm.height() + _lineSpacing;

    Q_ASSERT(_fontHeight > 0);

    /* TODO: When changing the three deprecated width() below
     *       consider the info in
     *       https://phabricator.kde.org/D23144 comments
     *       horizontalAdvance() was added in Qt 5.11 (which should be the
     *       minimum for 20.04 or 20.08 KDE Applications release)
     */

    // waba TerminalDisplay 1.123:
    // "Base character width on widest ASCII character. This prevents too wide
    //  characters in the presence of double wide (e.g. Japanese) characters."
    // Get the width from representative normal width characters
    _fontWidth = qRound((static_cast<double>(fm.horizontalAdvance(QStringLiteral(REPCHAR))) / static_cast<double>(qstrlen(REPCHAR))));

    _fixedFont = true;

    const int fw = fm.horizontalAdvance(QLatin1Char(REPCHAR[0]));
    for (unsigned int i = 1; i < qstrlen(REPCHAR); i++) {
        if (fw != fm.horizontalAdvance(QLatin1Char(REPCHAR[i]))) {
            _fixedFont = false;
            break;
        }
    }

    if (_fontWidth < 1) {
        _fontWidth = 1;
    }

    _fontAscent = fm.ascent();

    emit changedFontMetricSignal(_fontHeight, _fontWidth);
    propagateSize();
    update();
}

void TerminalDisplay::setVTFont(const QFont& f)
{
    QFont newFont(f);
    int strategy = 0;

    // hint that text should be drawn with- or without anti-aliasing.
    // depending on the user's font configuration, this may not be respected
    strategy |= _antialiasText ? QFont::PreferAntialias : QFont::NoAntialias;

    // Konsole cannot handle non-integer font metrics
    strategy |= QFont::ForceIntegerMetrics;

    // In case the provided font doesn't have some specific characters it should
    // fall back to a Monospace fonts.
    newFont.setStyleHint(QFont::TypeWriter, QFont::StyleStrategy(strategy));

    // Try to check that a good font has been loaded.
    // For some fonts, ForceIntegerMetrics causes height() == 0 which
    // will cause Konsole to crash later.
    QFontMetrics fontMetrics2(newFont);
    if (fontMetrics2.height() < 1) {
        qCDebug(KonsoleDebug)<<"The font "<<newFont.toString()<<" has an invalid height()";
        // Ask for a generic font so at least it is usable.
        // Font listed in profile's dialog will not be updated.
        newFont = QFont(QStringLiteral("Monospace"));
        // Set style strategy without ForceIntegerMetrics for the font
        strategy &= ~QFont::ForceIntegerMetrics;
        newFont.setStyleHint(QFont::TypeWriter, QFont::StyleStrategy(strategy));
        qCDebug(KonsoleDebug)<<"Font changed to "<<newFont.toString();
    }

    // experimental optimization.  Konsole assumes that the terminal is using a
    // mono-spaced font, in which case kerning information should have an effect.
    // Disabling kerning saves some computation when rendering text.
    newFont.setKerning(false);

    // "Draw intense colors in bold font" feature needs to use different font weights. StyleName
    // property, when set, doesn't allow weight changes. Since all properties (weight, stretch,
    // italic, etc) are stored in QFont independently, in almost all cases styleName is not needed.
    newFont.setStyleName(QString());

    if (newFont == font()) {
        // Do not process the same font again
        return;
    }

    QFontInfo fontInfo(newFont);

    // QFontInfo::fixedPitch() appears to not match QFont::fixedPitch() - do not test it.
    // related?  https://bugreports.qt.io/browse/QTBUG-34082
    if (fontInfo.family() != newFont.family()
            || !qFuzzyCompare(fontInfo.pointSizeF(), newFont.pointSizeF())
            || fontInfo.styleHint()  != newFont.styleHint()
            || fontInfo.weight()     != newFont.weight()
            || fontInfo.style()      != newFont.style()
            || fontInfo.underline()  != newFont.underline()
            || fontInfo.strikeOut()  != newFont.strikeOut()
            || fontInfo.rawMode()    != newFont.rawMode()) {
        const QString nonMatching = QString::asprintf("%s,%g,%d,%d,%d,%d,%d,%d,%d,%d",
                qPrintable(fontInfo.family()),
                fontInfo.pointSizeF(),
                -1, // pixelSize is not used
                static_cast<int>(fontInfo.styleHint()),
                fontInfo.weight(),
                static_cast<int>(fontInfo.style()),
                static_cast<int>(fontInfo.underline()),
                static_cast<int>(fontInfo.strikeOut()),
                // Intentional newFont use - fixedPitch is bugged, see comment above
                static_cast<int>(newFont.fixedPitch()),
                static_cast<int>(fontInfo.rawMode()));
        qCDebug(KonsoleDebug) << "The font to use in the terminal can not be matched exactly on your system.";
        qCDebug(KonsoleDebug) << " Selected: " << newFont.toString();
        qCDebug(KonsoleDebug) << " System  : " << nonMatching;
    }

    QWidget::setFont(newFont);
    fontChange(newFont);
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

void TerminalDisplay::resetFontSize()
{
    const qreal MinimumFontSize = 6;

    QFont font = getVTFont();
    Profile::Ptr currentProfile = SessionManager::instance()->sessionProfile(_sessionController->session());
    const qreal defaultFontSize = currentProfile->font().pointSizeF();

    font.setPointSizeF(qMax(defaultFontSize, MinimumFontSize));
    setVTFont(font);
}

uint TerminalDisplay::lineSpacing() const
{
    return _lineSpacing;
}

void TerminalDisplay::setLineSpacing(uint i)
{
    _lineSpacing = i;
    fontChange(font()); // Trigger an update.
}


/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Accessibility                                     */
/*                                                                           */
/* ------------------------------------------------------------------------- */

namespace Konsole
{

#ifndef QT_NO_ACCESSIBILITY
/**
 * This function installs the factory function which lets Qt instantiate the QAccessibleInterface
 * for the TerminalDisplay.
 */
QAccessibleInterface* accessibleInterfaceFactory(const QString &key, QObject *object)
{
    Q_UNUSED(key)
    if (auto *display = qobject_cast<TerminalDisplay*>(object)) {
        return new TerminalDisplayAccessible(display);
    }
    return nullptr;
}

#endif
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                         Constructor / Destructor                          */
/*                                                                           */
/* ------------------------------------------------------------------------- */

TerminalDisplay::TerminalDisplay(QWidget* parent)
    : QWidget(parent)
    , _screenWindow(nullptr)
    , _bellMasked(false)
    , _verticalLayout(new QVBoxLayout(this))
    , _fixedFont(true)
    , _fontHeight(1)
    , _fontWidth(1)
    , _fontAscent(1)
    , _boldIntense(true)
    , _lines(1)
    , _columns(1)
    , _usedLines(1)
    , _usedColumns(1)
    , _contentRect(QRect())
    , _image(nullptr)
    , _imageSize(0)
    , _lineProperties(QVector<LineProperty>())
    , _randomSeed(0)
    , _resizing(false)
    , _showTerminalSizeHint(true)
    , _bidiEnabled(false)
    , _usesMouseTracking(false)
    , _alternateScrolling(true)
    , _bracketedPasteMode(false)
    , _iPntSel(QPoint())
    , _pntSel(QPoint())
    , _tripleSelBegin(QPoint())
    , _actSel(0)
    , _wordSelectionMode(false)
    , _lineSelectionMode(false)
    , _preserveLineBreaks(true)
    , _columnSelectionMode(false)
    , _autoCopySelectedText(false)
    , _copyTextAsHTML(true)
    , _middleClickPasteMode(Enum::PasteFromX11Selection)
    , _scrollBar(nullptr)
    , _scrollbarLocation(Enum::ScrollBarRight)
    , _scrollFullPage(false)
    , _wordCharacters(QStringLiteral(":@-./_~"))
    , _bellMode(Enum::NotifyBell)
    , _allowBlinkingText(true)
    , _allowBlinkingCursor(false)
    , _textBlinking(false)
    , _cursorBlinking(false)
    , _hasTextBlinker(false)
    , _urlHintsModifiers(Qt::NoModifier)
    , _showUrlHint(false)
    , _reverseUrlHints(false)
    , _openLinksByDirectClick(false)
    , _ctrlRequiredForDrag(true)
    , _dropUrlsAsText(false)
    , _tripleClickMode(Enum::SelectWholeLine)
    , _possibleTripleClick(false)
    , _resizeWidget(nullptr)
    , _resizeTimer(nullptr)
    , _flowControlWarningEnabled(false)
    , _outputSuspendedMessageWidget(nullptr)
    , _lineSpacing(0)
    , _size(QSize())
    , _blendColor(qRgba(0, 0, 0, 0xff))
    , _wallpaper(nullptr)
    , _filterChain(new TerminalImageFilterChain())
    , _mouseOverHotspotArea(QRegion())
    , _filterUpdateRequired(true)
    , _cursorShape(Enum::BlockCursor)
    , _cursorColor(QColor())
    , _cursorTextColor(QColor())
    , _antialiasText(true)
    , _useFontLineCharacters(false)
    , _printerFriendly(false)
    , _sessionController(nullptr)
    , _trimLeadingSpaces(false)
    , _trimTrailingSpaces(false)
    , _mouseWheelZoom(false)
    , _margin(1)
    , _centerContents(false)
    , _readOnlyMessageWidget(nullptr)
    , _readOnly(false)
    , _opacity(1.0)
    , _dimWhenInactive(false)
    , _scrollWheelState(ScrollState())
    , _searchBar(new IncrementalSearchBar(this))
    , _headerBar(new TerminalHeaderBar(this))
    , _searchResultRect(QRect())
    , _drawOverlay(false)
{
    // terminal applications are not designed with Right-To-Left in mind,
    // so the layout is forced to Left-To-Right
    setLayoutDirection(Qt::LeftToRight);

    _contentRect = QRect(_margin, _margin, 1, 1);

    // create scroll bar for scrolling output up and down
    _scrollBar = new QScrollBar(this);
    _scrollBar->setAutoFillBackground(false);
    // set the scroll bar's slider to occupy the whole area of the scroll bar initially
    setScroll(0, 0);
    _scrollBar->setCursor(Qt::ArrowCursor);
    _headerBar->setCursor(Qt::ArrowCursor);
    connect(_headerBar, &TerminalHeaderBar::requestToggleExpansion, this, &Konsole::TerminalDisplay::requestToggleExpansion);
    connect(_scrollBar, &QScrollBar::valueChanged, this, &Konsole::TerminalDisplay::scrollBarPositionChanged);
    connect(_scrollBar, &QScrollBar::sliderMoved, this, &Konsole::TerminalDisplay::viewScrolledByUser);

    // setup timers for blinking text
    _blinkTextTimer = new QTimer(this);
    _blinkTextTimer->setInterval(TEXT_BLINK_DELAY);
    connect(_blinkTextTimer, &QTimer::timeout, this, &Konsole::TerminalDisplay::blinkTextEvent);

    // setup timers for blinking cursor
    _blinkCursorTimer = new QTimer(this);
    _blinkCursorTimer->setInterval(QApplication::cursorFlashTime() / 2);
    connect(_blinkCursorTimer, &QTimer::timeout, this, &Konsole::TerminalDisplay::blinkCursorEvent);

    // hide mouse cursor on keystroke or idle
    KCursor::setAutoHideCursor(this, true);
    setMouseTracking(true);

    setUsesMouseTracking(false);
    setBracketedPasteMode(false);

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

    // Add the stretch item once, the KMessageWidgets are inserted at index 0.
    _verticalLayout->addWidget(_headerBar);
    _verticalLayout->addStretch();
    _verticalLayout->setSpacing(0);
    _verticalLayout->setContentsMargins(0, 0, 0, 0);
    setLayout(_verticalLayout);
    new AutoScrollHandler(this);

    // Keep this last
    auto focusWatcher = new CompositeWidgetFocusWatcher(this, this);
    connect(focusWatcher, &CompositeWidgetFocusWatcher::compositeFocusChanged,
            this, [this](bool focused) {_hasCompositeFocus = focused;});
    connect(focusWatcher, &CompositeWidgetFocusWatcher::compositeFocusChanged,
            this, &TerminalDisplay::compositeFocusChanged);
    connect(focusWatcher, &CompositeWidgetFocusWatcher::compositeFocusChanged,
            _headerBar, &TerminalHeaderBar::setFocusIndicatorState);

#ifndef QT_NO_ACCESSIBILITY
    QAccessible::installFactory(Konsole::accessibleInterfaceFactory);
#endif

    connect(KonsoleSettings::self(), &KonsoleSettings::configChanged, this, &TerminalDisplay::setupHeaderVisibility);
}

TerminalDisplay::~TerminalDisplay()
{
    disconnect(_blinkTextTimer);
    disconnect(_blinkCursorTimer);

    delete _readOnlyMessageWidget;
    delete _outputSuspendedMessageWidget;
    delete[] _image;
    delete _filterChain;

    _readOnlyMessageWidget = nullptr;
    _outputSuspendedMessageWidget = nullptr;
}

void TerminalDisplay::setupHeaderVisibility()
{
    _headerBar->applyVisibilitySettings();
    calcGeometry();
}

void TerminalDisplay::hideDragTarget()
{
    _drawOverlay = false;
    update();
}

void TerminalDisplay::showDragTarget(const QPoint& cursorPos)
{
    using EdgeDistance = std::pair<int, Qt::Edge>;
    auto closerToEdge = std::min<EdgeDistance>(
        {
            {cursorPos.x(), Qt::LeftEdge},
            {cursorPos.y(), Qt::TopEdge},
            {width() - cursorPos.x(), Qt::RightEdge},
            {height() - cursorPos.y(), Qt::BottomEdge}
        },
        [](const EdgeDistance& left, const EdgeDistance& right) -> bool {
            return left.first < right.first;
        }
    );
    if (_overlayEdge == closerToEdge.second) {
        return;
    }
    _overlayEdge = closerToEdge.second;
    _drawOverlay = true;
    update();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Display Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::drawLineCharString(QPainter& painter, int x, int y, const QString& str,
        const Character* attributes)
{
    // only turn on anti-aliasing during this short time for the "text"
    // for the normal text we have TextAntialiasing on demand on
    // otherwise we have rendering artifacts
    // set https://bugreports.qt.io/browse/QTBUG-66036
    painter.setRenderHint(QPainter::Antialiasing, _antialiasText);

    const bool useBoldPen = (attributes->rendition & RE_BOLD) != 0 && _boldIntense;

    QRect cellRect = {x, y, _fontWidth, _fontHeight};
    for (int i = 0 ; i < str.length(); i++) {
        LineBlockCharacters::draw(painter, cellRect.translated(i * _fontWidth, 0), str[i],
                                        useBoldPen);
    }

    painter.setRenderHint(QPainter::Antialiasing, false);
}

void TerminalDisplay::setKeyboardCursorShape(Enum::CursorShapeEnum shape)
{
    _cursorShape = shape;
}

void TerminalDisplay::setCursorStyle(Enum::CursorShapeEnum shape, bool isBlinking)
{
    setKeyboardCursorShape(shape);

    setBlinkingCursorEnabled(isBlinking);

    // when the cursor shape and blinking state are changed via the
    // Set Cursor Style (DECSCUSR) escape sequences in vim, and if the
    // cursor isn't set to blink, the cursor shape doesn't actually
    // change until the cursor is moved by the user; calling update()
    // makes the cursor shape get updated sooner.
    if (!isBlinking) {
        update();
    }
}
void TerminalDisplay::resetCursorStyle()
{
    Q_ASSERT(_sessionController != nullptr);
    Q_ASSERT(!_sessionController->session().isNull());

    Profile::Ptr currentProfile = SessionManager::instance()->sessionProfile(_sessionController->session());

    if (currentProfile != nullptr) {
        auto shape = static_cast<Enum::CursorShapeEnum>(currentProfile->property<int>(Profile::CursorShape));

        setKeyboardCursorShape(shape);
        setBlinkingCursorEnabled(currentProfile->blinkingCursorEnabled());
    }
}

void TerminalDisplay::setKeyboardCursorColor(const QColor& color)
{
    _cursorColor = color;
}

void TerminalDisplay::setKeyboardCursorTextColor(const QColor& color)
{
    _cursorTextColor = color;
}

void TerminalDisplay::setOpacity(qreal opacity)
{
    QColor color(_blendColor);
    color.setAlphaF(opacity);
    _opacity = opacity;

    _blendColor = color.rgba();
    onColorsChanged();
}

void TerminalDisplay::setWallpaper(const ColorSchemeWallpaper::Ptr &p)
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

    if (useOpacitySetting && !_wallpaper->isNull() &&
            _wallpaper->draw(painter, rect, _opacity)) {
    } else if (qAlpha(_blendColor) < 0xff && useOpacitySetting) {
#if defined(Q_OS_MACOS)
        // TODO - On MacOS, using CompositionMode doesn't work.  Altering the
        //        transparency in the color scheme alters the brightness.
        painter.fillRect(rect, backgroundColor);
#else
        QColor color(backgroundColor);
        color.setAlpha(qAlpha(_blendColor));

        const QPainter::CompositionMode originalMode = painter.compositionMode();
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.fillRect(rect, color);
        painter.setCompositionMode(originalMode);
#endif
    } else {
        painter.fillRect(rect, backgroundColor);
    }
}

void TerminalDisplay::drawCursor(QPainter& painter,
                                 const QRect& rect,
                                 const QColor& foregroundColor,
                                 const QColor& backgroundColor,
                                 QColor& characterColor)
{
    // don't draw cursor which is currently blinking
    if (_cursorBlinking) {
        return;
    }

    // shift rectangle top down one pixel to leave some space
    // between top and bottom
    QRectF cursorRect = rect.adjusted(0, 1, 0, 0);

    QColor cursorColor = _cursorColor.isValid() ? _cursorColor : foregroundColor;
    QPen pen(cursorColor);
    // TODO: the relative pen width to draw the cursor is a bit hacky
    // and set to 1/12 of the font width. Visually it seems to work at
    // all scales but there must be better ways to do it
    const qreal width = qMax(_fontWidth / 12.0, 1.0);
    const qreal halfWidth = width / 2.0;
    pen.setWidthF(width);
    painter.setPen(pen);

    if (_cursorShape == Enum::BlockCursor) {
        // draw the cursor outline, adjusting the area so that it is draw entirely inside 'rect'
        painter.drawRect(cursorRect.adjusted(halfWidth, halfWidth, -halfWidth, -halfWidth));

        // draw the cursor body only when the widget has focus
        if (hasFocus()) {
            painter.fillRect(cursorRect, cursorColor);

            // if the cursor text color is valid then use it to draw the character under the cursor,
            // otherwise invert the color used to draw the text to ensure that the character at
            // the cursor position is readable
            characterColor = _cursorTextColor.isValid() ? _cursorTextColor : backgroundColor;
        }
    } else if (_cursorShape == Enum::UnderlineCursor) {
        QLineF line(cursorRect.left() + halfWidth,
                    cursorRect.bottom() - halfWidth,
                    cursorRect.right() - halfWidth,
                    cursorRect.bottom() - halfWidth);
        painter.drawLine(line);

    } else if (_cursorShape == Enum::IBeamCursor) {
        QLineF line(cursorRect.left() + halfWidth,
                    cursorRect.top() + halfWidth,
                    cursorRect.left() + halfWidth,
                    cursorRect.bottom() - halfWidth);
        painter.drawLine(line);
    }
}

void TerminalDisplay::drawCharacters(QPainter& painter,
                                     const QRect& rect,
                                     const QString& text,
                                     const Character* style,
                                     const QColor& characterColor)
{
    // don't draw text which is currently blinking
    if (_textBlinking && ((style->rendition & RE_BLINK) != 0)) {
        return;
    }

    // don't draw concealed characters
    if ((style->rendition & RE_CONCEAL) != 0) {
        return;
    }

    static constexpr int MaxFontWeight = 99; // https://doc.qt.io/qt-5/qfont.html#Weight-enum

    const int normalWeight = font().weight();
    // +26 makes "bold" from "normal", "normal" from "light", etc. It is 26 instead of not 25 to prefer
    // bolder weight when 25 falls in the middle between two weights. See QFont::Weight
    const int boldWeight = qMin(normalWeight + 26, MaxFontWeight);

    const auto isBold = [boldWeight](const QFont &font) { return font.weight() >= boldWeight; };

    const bool useBold = (((style->rendition & RE_BOLD) != 0) && _boldIntense);
    const bool useUnderline = ((style->rendition & RE_UNDERLINE) != 0) || font().underline();
    const bool useItalic = ((style->rendition & RE_ITALIC) != 0) || font().italic();
    const bool useStrikeOut = ((style->rendition & RE_STRIKEOUT) != 0) || font().strikeOut();
    const bool useOverline = ((style->rendition & RE_OVERLINE) != 0) || font().overline();

    QFont currentFont = painter.font();

    if (isBold(currentFont) != useBold
            || currentFont.underline() != useUnderline
            || currentFont.italic() != useItalic
            || currentFont.strikeOut() != useStrikeOut
            || currentFont.overline() != useOverline) {
        currentFont.setWeight(useBold ? boldWeight : normalWeight);
        currentFont.setUnderline(useUnderline);
        currentFont.setItalic(useItalic);
        currentFont.setStrikeOut(useStrikeOut);
        currentFont.setOverline(useOverline);
        painter.setFont(currentFont);
    }

    // setup pen
    const QColor foregroundColor = style->foregroundColor.color(_colorTable);
    const QColor color = characterColor.isValid() ? characterColor : foregroundColor;
    QPen pen = painter.pen();
    if (pen.color() != color) {
        pen.setColor(color);
        painter.setPen(color);
    }

    const bool origClipping = painter.hasClipping();
    const auto origClipRegion = painter.clipRegion();
    painter.setClipRect(rect);
    // draw text
    if (isLineCharString(text) && !_useFontLineCharacters) {
        drawLineCharString(painter, rect.x(), rect.y(), text, style);
    } else {
        // Force using LTR as the document layout for the terminal area, because
        // there is no use cases for RTL emulator and RTL terminal application.
        //
        // This still allows RTL characters to be rendered in the RTL way.
        painter.setLayoutDirection(Qt::LeftToRight);

        if (_bidiEnabled) {
            painter.drawText(rect.x(), rect.y() + _fontAscent + _lineSpacing, text);
        } else {
            painter.drawText(rect.x(), rect.y() + _fontAscent + _lineSpacing, LTR_OVERRIDE_CHAR + text);
        }
    }
    painter.setClipRegion(origClipRegion);
    painter.setClipping(origClipping);
}

void TerminalDisplay::drawTextFragment(QPainter& painter ,
                                       const QRect& rect,
                                       const QString& text,
                                       const Character* style)
{
    // setup painter
    const QColor foregroundColor = style->foregroundColor.color(_colorTable);
    const QColor backgroundColor = style->backgroundColor.color(_colorTable);

    // draw background if different from the display's background color
    if (backgroundColor != getBackgroundColor()) {
        drawBackground(painter, rect, backgroundColor,
                       false /* do not use transparency */);
    }

    // draw cursor shape if the current character is the cursor
    // this may alter the foreground and background colors
    QColor characterColor;
    if ((style->rendition & RE_CURSOR) != 0) {
        drawCursor(painter, rect, foregroundColor, backgroundColor, characterColor);
    }

    // draw text
    drawCharacters(painter, rect, text, style, characterColor);
}

void TerminalDisplay::drawPrinterFriendlyTextFragment(QPainter& painter,
        const QRect& rect,
        const QString& text,
        const Character* style)
{
    // Set the colors used to draw to black foreground and white
    // background for printer friendly output when printing
    Character print_style = *style;
    print_style.foregroundColor = CharacterColor(COLOR_SPACE_RGB, 0x00000000);
    print_style.backgroundColor = CharacterColor(COLOR_SPACE_RGB, 0xFFFFFFFF);

    // draw text
    drawCharacters(painter, rect, text, &print_style, QColor());
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
    // return if there is nothing to do
    if ((lines == 0) || (_image == nullptr)) {
        return;
    }

    // if the flow control warning is enabled this will interfere with the
    // scrolling optimizations and cause artifacts.  the simple solution here
    // is to just disable the optimization whilst it is visible
    if ((_outputSuspendedMessageWidget != nullptr) && _outputSuspendedMessageWidget->isVisible()) {
        return;
    }

    if ((_readOnlyMessageWidget != nullptr) && _readOnlyMessageWidget->isVisible()) {
        return;
    }

    // constrain the region to the display
    // the bottom of the region is capped to the number of lines in the display's
    // internal image - 2, so that the height of 'region' is strictly less
    // than the height of the internal image.
    QRect region = screenWindowRegion;
    region.setBottom(qMin(region.bottom(), _lines - 2));

    // return if there is nothing to do
    if (!region.isValid()
            || (region.top() + abs(lines)) >= region.bottom()
            || _lines <= region.height()) {
        return;
    }

    // hide terminal size label to prevent it being scrolled
    if ((_resizeWidget != nullptr) && _resizeWidget->isVisible()) {
        _resizeWidget->hide();
    }

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
        scrollRect.setLeft(scrollBarWidth + SCROLLBAR_CONTENT_GAP + (_highlightScrolledLinesControl.enabled ? HIGHLIGHT_SCROLLED_LINES_WIDTH : 0));
        scrollRect.setRight(width());
    } else {
        scrollRect.setLeft(_highlightScrolledLinesControl.enabled ? HIGHLIGHT_SCROLLED_LINES_WIDTH : 0);
        scrollRect.setRight(width() - scrollBarWidth - SCROLLBAR_CONTENT_GAP);
    }
    void* firstCharPos = &_image[ region.top() * _columns ];
    void* lastCharPos = &_image[(region.top() + abs(lines)) * _columns ];

    const int top = _contentRect.top() + (region.top() * _fontHeight);
    const int linesToMove = region.height() - abs(lines);
    const int bytesToMove = linesToMove * _columns * sizeof(Character);

    Q_ASSERT(linesToMove > 0);
    Q_ASSERT(bytesToMove > 0);

    scrollRect.setTop( lines > 0 ? top : top + abs(lines) * _fontHeight);
    scrollRect.setHeight(linesToMove * _fontHeight);

    if (!scrollRect.isValid() || scrollRect.isEmpty()) {
        return;
    }

    //scroll internal image
    if (lines > 0) {
        // check that the memory areas that we are going to move are valid
        Q_ASSERT((char*)lastCharPos + bytesToMove <
                 (char*)(_image + (_lines * _columns)));

        Q_ASSERT((lines * _columns) < _imageSize);

        //scroll internal image down
        memmove(firstCharPos , lastCharPos , bytesToMove);
    } else {
        // check that the memory areas that we are going to move are valid
        Q_ASSERT((char*)firstCharPos + bytesToMove <
                 (char*)(_image + (_lines * _columns)));

        //scroll internal image up
        memmove(lastCharPos , firstCharPos , bytesToMove);
    }

    //scroll the display vertically to match internal _image
    scroll(0 , _fontHeight * (-lines) , scrollRect);
}

QRegion TerminalDisplay::hotSpotRegion() const
{
    QRegion region;
    for (const auto &hotSpot : _filterChain->hotSpots()) {
        QRect r;
        r.setLeft(hotSpot->startColumn());
        r.setTop(hotSpot->startLine());
        if (hotSpot->startLine() == hotSpot->endLine()) {
            r.setRight(hotSpot->endColumn());
            r.setBottom(hotSpot->endLine());
            region |= imageToWidget(r);
        } else {
            r.setRight(_columns);
            r.setBottom(hotSpot->startLine());
            region |= imageToWidget(r);

            r.setLeft(0);

            for (int line = hotSpot->startLine() + 1 ; line < hotSpot->endLine(); line++) {
                r.moveTop(line);
                region |= imageToWidget(r);
            }

            r.moveTop(hotSpot->endLine());
            r.setRight(hotSpot->endColumn());
            region |= imageToWidget(r);
        }
    }
    return region;
}

void TerminalDisplay::processFilters()
{

    if (_screenWindow.isNull()) {
        return;
    }

    if (!_filterUpdateRequired) {
        return;
    }

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
    _filterUpdateRequired = false;
}

void TerminalDisplay::updateImage()
{
    if (_screenWindow.isNull()) {
        return;
    }

    // optimization - scroll the existing image where possible and
    // avoid expensive text drawing for parts of the image that
    // can simply be moved up or down
    // disable this shortcut for transparent konsole with scaled pixels, otherwise we get rendering artifacts, see BUG 350651
    if (!(WindowSystemInfo::HAVE_TRANSPARENCY && (qApp->devicePixelRatio() > 1.0)) && _wallpaper->isNull() && !_searchBar->isVisible()) {
        scrollImage(_screenWindow->scrollCount() ,
                    _screenWindow->scrollRegion());
    }

    if (_image == nullptr) {
        // Create _image.
        // The emitted changedContentSizeSignal also leads to getImage being recreated, so do this first.
        updateImageSize();
    }

    Character* const newimg = _screenWindow->getImage();
    const int lines = _screenWindow->windowLines();
    const int columns = _screenWindow->windowColumns();

    setScroll(_screenWindow->currentLine() , _screenWindow->lineCount());

    Q_ASSERT(_usedLines <= _lines);
    Q_ASSERT(_usedColumns <= _columns);

    int y, x, len;

    const QPoint tL  = contentsRect().topLeft();
    const int    tLx = tL.x();
    const int    tLy = tL.y();
    _hasTextBlinker = false;

    CharacterColor cf;       // undefined

    const int linesToUpdate = qMin(_lines, qMax(0, lines));
    const int columnsToUpdate = qMin(_columns, qMax(0, columns));

    auto dirtyMask = new char[columnsToUpdate + 2];
    QRegion dirtyRegion;

    // debugging variable, this records the number of lines that are found to
    // be 'dirty' ( ie. have changed from the old _image to the new _image ) and
    // which therefore need to be repainted
    int dirtyLineCount = 0;

    for (y = 0; y < linesToUpdate; ++y) {
        const Character* currentLine = &_image[y * _columns];
        const Character* const newLine = &newimg[y * columns];

        bool updateLine = false;

        // The dirty mask indicates which characters need repainting. We also
        // mark surrounding neighbors dirty, in case the character exceeds
        // its cell boundaries
        memset(dirtyMask, 0, columnsToUpdate + 2);

        for (x = 0 ; x < columnsToUpdate ; ++x) {
            if (newLine[x] != currentLine[x]) {
                dirtyMask[x] = 1;
            }
        }

        if (!_resizing) { // not while _resizing, we're expecting a paintEvent
            for (x = 0; x < columnsToUpdate; ++x) {
                _hasTextBlinker |= (newLine[x].rendition & RE_BLINK);

                // Start drawing if this character or the next one differs.
                // We also take the next one into account to handle the situation
                // where characters exceed their cell width.
                if (dirtyMask[x] != 0) {
                    if (newLine[x + 0].character == 0u) {
                        continue;
                    }
                    const bool lineDraw = LineBlockCharacters::canDraw(newLine[x + 0].character);
                    const bool doubleWidth = (x + 1 == columnsToUpdate) ? false : (newLine[x + 1].character == 0);
                    const RenditionFlags cr = newLine[x].rendition;
                    const CharacterColor clipboard = newLine[x].backgroundColor;
                    if (newLine[x].foregroundColor != cf) {
                        cf = newLine[x].foregroundColor;
                    }
                    const int lln = columnsToUpdate - x;
                    for (len = 1; len < lln; ++len) {
                        const Character& ch = newLine[x + len];

                        if (ch.character == 0u) {
                            continue; // Skip trailing part of multi-col chars.
                        }

                        const bool nextIsDoubleWidth = (x + len + 1 == columnsToUpdate) ? false : (newLine[x + len + 1].character == 0);

                        if (ch.foregroundColor != cf ||
                                ch.backgroundColor != clipboard ||
                                (ch.rendition & ~RE_EXTENDED_CHAR) != (cr & ~RE_EXTENDED_CHAR) ||
                                (dirtyMask[x + len] == 0) ||
                                LineBlockCharacters::canDraw(ch.character) != lineDraw ||
                                nextIsDoubleWidth != doubleWidth) {
                            break;
                        }
                    }

                    const bool saveFixedFont = _fixedFont;
                    if (lineDraw) {
                        _fixedFont = false;
                    }
                    if (doubleWidth) {
                        _fixedFont = false;
                    }

                    updateLine = true;

                    _fixedFont = saveFixedFont;
                    x += len - 1;
                }
            }
        }

        //both the top and bottom halves of double height _lines must always be redrawn
        //although both top and bottom halves contain the same characters, only
        //the top one is actually
        //drawn.
        if (_lineProperties.count() > y) {
            updateLine |= (_lineProperties[y] & LINE_DOUBLEHEIGHT);
        }

        // if the characters on the line are different in the old and the new _image
        // then this line must be repainted.
        if (updateLine) {
            dirtyLineCount++;

            // add the area occupied by this line to the region which needs to be
            // repainted
            QRect dirtyRect = QRect(_contentRect.left() + tLx ,
                                    _contentRect.top() + tLy + _fontHeight * y ,
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
        dirtyRegion |= QRect(_contentRect.left() + tLx ,
                             _contentRect.top() + tLy + _fontHeight * linesToUpdate ,
                             _fontWidth * _columns ,
                             _fontHeight * (_usedLines - linesToUpdate));
    }
    _usedLines = linesToUpdate;

    if (columnsToUpdate < _usedColumns) {
        dirtyRegion |= QRect(_contentRect.left() + tLx + columnsToUpdate * _fontWidth ,
                             _contentRect.top() + tLy ,
                             _fontWidth * (_usedColumns - columnsToUpdate) ,
                             _fontHeight * _lines);
    }
    _usedColumns = columnsToUpdate;

    dirtyRegion |= _inputMethodData.previousPreeditRect;

    if ((_screenWindow->currentResultLine() != -1) && (_screenWindow->scrollCount() != 0)) {
        // De-highlight previous result region
        dirtyRegion |= _searchResultRect;
        // Highlight new result region
        dirtyRegion |= QRect(0, _contentRect.top() + (_screenWindow->currentResultLine() - _screenWindow->currentLine()) * _fontHeight,
                             _columns * _fontWidth, _fontHeight);
    }

    if (_highlightScrolledLinesControl.enabled) {
        dirtyRegion |= highlightScrolledLinesRegion(dirtyRegion.isEmpty());
    }
    _screenWindow->resetScrollCount();

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

#ifndef QT_NO_ACCESSIBILITY
    QAccessibleEvent dataChangeEvent(this, QAccessible::VisibleDataChanged);
    QAccessible::updateAccessibility(&dataChangeEvent);
    QAccessibleTextCursorEvent cursorEvent(this, _usedColumns * screenWindow()->screen()->getCursorY() + screenWindow()->screen()->getCursorX());
    QAccessible::updateAccessibility(&cursorEvent);
#endif
}

void TerminalDisplay::showResizeNotification()
{
    if (_showTerminalSizeHint && isVisible()) {
        if (_resizeWidget == nullptr) {
            _resizeWidget = new QLabel(i18n("Size: XXX x XXX"), this);
            _resizeWidget->setMinimumWidth(_resizeWidget->fontMetrics().boundingRect(i18n("Size: XXX x XXX")).width());
            _resizeWidget->setMinimumHeight(_resizeWidget->sizeHint().height());
            _resizeWidget->setAlignment(Qt::AlignCenter);

            _resizeWidget->setStyleSheet(QStringLiteral("background-color:palette(window);border-style:solid;border-width:1px;border-color:palette(dark)"));

            _resizeTimer = new QTimer(this);
            _resizeTimer->setInterval(SIZE_HINT_DURATION);
            _resizeTimer->setSingleShot(true);
            connect(_resizeTimer, &QTimer::timeout, _resizeWidget, &QLabel::hide);
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

    // Determine which characters should be repainted (1 region unit = 1 character)
    QRegion dirtyImageRegion;
    const QRegion region = pe->region() & contentsRect();

    for (const QRect &rect : region) {
        dirtyImageRegion += widgetToImage(rect);
        drawBackground(paint, rect, getBackgroundColor(), true /* use opacity setting */);
    }

    // only turn on text anti-aliasing, never turn on normal antialiasing
    // set https://bugreports.qt.io/browse/QTBUG-66036
    paint.setRenderHint(QPainter::TextAntialiasing, _antialiasText);

    for (const QRect &rect : qAsConst(dirtyImageRegion)) {
        drawContents(paint, rect);
    }
    drawCurrentResultRect(paint);
    highlightScrolledLines(paint);
    drawInputMethodPreeditString(paint, preeditRect());
    paintFilters(paint);

    const bool drawDimmed = _dimWhenInactive && !hasFocus();
    const QColor dimColor(0, 0, 0, 128);
    for (const QRect &rect : region) {
        if (drawDimmed) {
            paint.fillRect(rect, dimColor);
        }
    }

    if (_drawOverlay) {
        const auto y = _headerBar->isVisible() ? _headerBar->height() : 0;
        const auto rect = _overlayEdge == Qt::LeftEdge ? QRect(0, y, width() / 2, height())
                  : _overlayEdge == Qt::TopEdge ? QRect(0, y, width(), height() / 2)
                  : _overlayEdge == Qt::RightEdge ? QRect(width() - width() / 2, y, width() / 2, height())
                  : QRect(0, height() - height() / 2, width(), height() / 2);

        paint.setRenderHint(QPainter::Antialiasing);
        paint.setPen(Qt::NoPen);
        paint.setBrush(QColor(100,100,100, 127));
        paint.drawRect(rect);
    }
}

void TerminalDisplay::printContent(QPainter& painter, bool friendly)
{
    // Reinitialize the font with the printers paint device so the font
    // measurement calculations will be done correctly
    QFont savedFont = getVTFont();
    QFont font(savedFont, painter.device());
    painter.setFont(font);
    setVTFont(font);

    QRect rect(0, 0, _usedColumns, _usedLines);

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
    if (!_screenWindow.isNull()) {
        return _screenWindow->cursorPosition();
    } else {
        return {0, 0};
    }
}

inline bool TerminalDisplay::isCursorOnDisplay() const
{
    return cursorPosition().x() < _columns &&
           cursorPosition().y() < _lines;
}

FilterChain* TerminalDisplay::filterChain() const
{
    return _filterChain;
}

void TerminalDisplay::paintFilters(QPainter& painter)
{
    if (_filterUpdateRequired) {
        return;
    }

    // get color of character under mouse and use it to draw
    // lines for filters
    QPoint cursorPos = mapFromGlobal(QCursor::pos());
    int cursorLine;
    int cursorColumn;

    getCharacterPosition(cursorPos, cursorLine, cursorColumn, false);
    Character cursorCharacter = _image[loc(qMin(cursorColumn, _columns - 1), cursorLine)];

    painter.setPen(QPen(cursorCharacter.foregroundColor.color(_colorTable)));

    // iterate over hotspots identified by the display's currently active filters
    // and draw appropriate visuals to indicate the presence of the hotspot

    const auto spots = _filterChain->hotSpots();
    int urlNumber = 0, urlNumInc;
    if (_reverseUrlHints) {
        for (const auto &spot : spots) {
            if (spot->type() == Filter::HotSpot::Link) {
                urlNumber++;
            }
        }

        urlNumInc = -1;
    } else {
        urlNumber = 1;
        urlNumInc = 1;
    }

    for (const auto &spot : spots) {
        QRegion region;
        if (spot->type() == Filter::HotSpot::Link || spot->type() == Filter::HotSpot::EMailAddress) {
            QRect r;
            if (spot->startLine() == spot->endLine()) {
                r.setCoords(spot->startColumn()*_fontWidth + _contentRect.left(),
                            spot->startLine()*_fontHeight + _contentRect.top(),
                            (spot->endColumn())*_fontWidth + _contentRect.left() - 1,
                            (spot->endLine() + 1)*_fontHeight + _contentRect.top() - 1);
                region |= r;
            } else {
                r.setCoords(spot->startColumn()*_fontWidth + _contentRect.left(),
                            spot->startLine()*_fontHeight + _contentRect.top(),
                            (_columns)*_fontWidth + _contentRect.left() - 1,
                            (spot->startLine() + 1)*_fontHeight + _contentRect.top() - 1);
                region |= r;
                for (int line = spot->startLine() + 1 ; line < spot->endLine() ; line++) {
                    r.setCoords(0 * _fontWidth + _contentRect.left(),
                                line * _fontHeight + _contentRect.top(),
                                (_columns)*_fontWidth + _contentRect.left() - 1,
                                (line + 1)*_fontHeight + _contentRect.top() - 1);
                    region |= r;
                }
                r.setCoords(0 * _fontWidth + _contentRect.left(),
                            spot->endLine()*_fontHeight + _contentRect.top(),
                            (spot->endColumn())*_fontWidth + _contentRect.left() - 1,
                            (spot->endLine() + 1)*_fontHeight + _contentRect.top() - 1);
                region |= r;
            }

            if (_showUrlHint && spot->type() == Filter::HotSpot::Link) {
                if (urlNumber >= 0 && urlNumber < 10) {
                // Position at the beginning of the URL
                QRect hintRect(*region.begin());
                hintRect.setWidth(r.height());
                painter.fillRect(hintRect, QColor(0, 0, 0, 128));
                painter.setPen(Qt::white);
                painter.drawRect(hintRect.adjusted(0, 0, -1, -1));
                painter.drawText(hintRect, Qt::AlignCenter, QString::number(urlNumber));
                }

                urlNumber += urlNumInc;
            }
        }

        for (int line = spot->startLine() ; line <= spot->endLine() ; line++) {
            int startColumn = 0;
            int endColumn = _columns - 1; // TODO use number of _columns which are actually
            // occupied on this line rather than the width of the
            // display in _columns

            // Check image size so _image[] is valid (see makeImage)
            if (endColumn >= _columns || line >= _lines) {
                break;
            }

            // ignore whitespace at the end of the lines
            while (_image[loc(endColumn, line)].isSpace() && endColumn > 0) {
                endColumn--;
            }

            // increment here because the column which we want to set 'endColumn' to
            // is the first whitespace character at the end of the line
            endColumn++;

            if (line == spot->startLine()) {
                startColumn = spot->startColumn();
            }
            if (line == spot->endLine()) {
                endColumn = spot->endColumn();
            }

            // TODO: resolve this comment with the new margin/center code
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
            r.setCoords(startColumn * _fontWidth + _contentRect.left(),
                        line * _fontHeight + _contentRect.top(),
                        endColumn * _fontWidth + _contentRect.left() - 1,
                        (line + 1)*_fontHeight + _contentRect.top() - 1);
            // Underline link hotspots
            const bool hasMouse = region.contains(mapFromGlobal(QCursor::pos()));
            if ((spot->type() == Filter::HotSpot::Link && _showUrlHint) || hasMouse) {
                QFontMetrics metrics(font());

                // find the baseline (which is the invisible line that the characters in the font sit on,
                // with some having tails dangling below)
                const int baseline = r.bottom() - metrics.descent();
                // find the position of the underline below that
                const int underlinePos = baseline + metrics.underlinePos();
                painter.drawLine(r.left() , underlinePos, r.right() , underlinePos);

                // Marker hotspots simply have a transparent rectangular shape
                // drawn on top of them
            } else if (spot->type() == Filter::HotSpot::Marker) {
                //TODO - Do not use a hardcoded color for this
                const bool isCurrentResultLine = (_screenWindow->currentResultLine() == (spot->startLine() + _screenWindow->currentLine()));
                QColor color = isCurrentResultLine ? QColor(255, 255, 0, 120) : QColor(255, 0, 0, 120);
                painter.fillRect(r, color);
            }
        }
    }
}

static uint baseCodePoint(const Character &ch) {
    if (ch.rendition & RE_EXTENDED_CHAR) {
        // sequence of characters
        ushort extendedCharLength = 0;
        const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(ch.character, extendedCharLength);
        return chars[0];
    } else {
        return ch.character;
    }
}

void TerminalDisplay::drawContents(QPainter& paint, const QRect& rect)
{
    const int numberOfColumns = _usedColumns;
    QVector<uint> univec;
    univec.reserve(numberOfColumns);
    for (int y = rect.y(); y <= rect.bottom(); y++) {
        int x = rect.x();
        if ((_image[loc(rect.x(), y)].character == 0u) && (x != 0)) {
            x--; // Search for start of multi-column character
        }
        for (; x <= rect.right(); x++) {
            int len = 1;
            int p = 0;

            // reset our buffer to the number of columns
            int bufferSize = numberOfColumns;
            univec.resize(bufferSize);
            uint *disstrU = univec.data();

            // is this a single character or a sequence of characters ?
            if ((_image[loc(x, y)].rendition & RE_EXTENDED_CHAR) != 0) {
                // sequence of characters
                ushort extendedCharLength = 0;
                const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(_image[loc(x, y)].character, extendedCharLength);
                if (chars != nullptr) {
                    Q_ASSERT(extendedCharLength > 1);
                    bufferSize += extendedCharLength - 1;
                    univec.resize(bufferSize);
                    disstrU = univec.data();
                    for (int index = 0 ; index < extendedCharLength ; index++) {
                        Q_ASSERT(p < bufferSize);
                        disstrU[p++] = chars[index];
                    }
                }
            } else {
                // single character
                const uint c = _image[loc(x, y)].character;
                if (c != 0u) {
                    Q_ASSERT(p < bufferSize);
                    disstrU[p++] = c;
                }
            }

            const bool lineDraw = LineBlockCharacters::canDraw(_image[loc(x, y)].character);
            const bool doubleWidth = (_image[qMin(loc(x, y) + 1, _imageSize - 1)].character == 0);
            const CharacterColor currentForeground = _image[loc(x, y)].foregroundColor;
            const CharacterColor currentBackground = _image[loc(x, y)].backgroundColor;
            const RenditionFlags currentRendition = _image[loc(x, y)].rendition;
            const QChar::Script currentScript = QChar::script(baseCodePoint(_image[loc(x, y)]));

            const auto isInsideDrawArea = [&](int column) { return column <= rect.right(); };
            const auto hasSameColors = [&](int column) {
                return _image[loc(column, y)].foregroundColor == currentForeground
                    && _image[loc(column, y)].backgroundColor == currentBackground;
            };
            const auto hasSameRendition = [&](int column) {
                return (_image[loc(column, y)].rendition & ~RE_EXTENDED_CHAR)
                    == (currentRendition & ~RE_EXTENDED_CHAR);
            };
            const auto hasSameWidth = [&](int column) {
                const int characterLoc = qMin(loc(column, y) + 1, _imageSize - 1);
                return (_image[characterLoc].character == 0) == doubleWidth;
            };
            const auto hasSameLineDrawStatus = [&](int column) {
                return LineBlockCharacters::canDraw(_image[loc(column, y)].character)
                    == lineDraw;
            };
            const auto isSameScript = [&](int column) {
                const QChar::Script script = QChar::script(baseCodePoint(_image[loc(column, y)]));
                if (currentScript == QChar::Script_Common || script == QChar::Script_Common
                    || currentScript == QChar::Script_Inherited || script == QChar::Script_Inherited) {
                    return true;
                }
                return currentScript == script;
            };
            const auto canBeGrouped = [&](int column) {
                return _image[loc(column, y)].character <= 0x7e
                       || (_image[loc(column, y)].rendition & RE_EXTENDED_CHAR)
                       || (_bidiEnabled && !doubleWidth);
            };

            if (canBeGrouped(x)) {
                while (isInsideDrawArea(x + len) && hasSameColors(x + len)
                       && hasSameRendition(x + len) && hasSameWidth(x + len)
                       && hasSameLineDrawStatus(x + len) && isSameScript(x + len)
                       && canBeGrouped(x + len)) {
                    const uint c = _image[loc(x + len, y)].character;
                    if ((_image[loc(x + len, y)].rendition & RE_EXTENDED_CHAR) != 0) {
                        // sequence of characters
                        ushort extendedCharLength = 0;
                        const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(c, extendedCharLength);
                        if (chars != nullptr) {
                            Q_ASSERT(extendedCharLength > 1);
                            bufferSize += extendedCharLength - 1;
                            univec.resize(bufferSize);
                            disstrU = univec.data();
                            for (int index = 0 ; index < extendedCharLength ; index++) {
                                Q_ASSERT(p < bufferSize);
                                disstrU[p++] = chars[index];
                            }
                        }
                    } else {
                        // single character
                        if (c != 0u) {
                            Q_ASSERT(p < bufferSize);
                            disstrU[p++] = c;
                        }
                    }

                    if (doubleWidth) { // assert((_image[loc(x+len,y)+1].character == 0)), see above if condition
                        len++; // Skip trailing part of multi-column character
                    }
                    len++;
                }
            } else {
                // Group spaces following any non-wide character with the character. This allows for
                // rendering ambiguous characters with wide glyphs without clipping them.
                while (!doubleWidth && isInsideDrawArea(x + len)
                        && _image[loc(x + len, y)].character == ' ' && hasSameColors(x + len)
                        && hasSameRendition(x + len)) {
                    // disstrU intentionally not modified - trailing spaces are meaningless
                    len++;
                }
            }
            if ((x + len < _usedColumns) && (_image[loc(x + len, y)].character == 0u)) {
                len++; // Adjust for trailing part of multi-column character
            }

            const bool save__fixedFont = _fixedFont;
            if (lineDraw) {
                _fixedFont = false;
            }
            if (doubleWidth) {
                _fixedFont = false;
            }
            univec.resize(p);

            // Create a text scaling matrix for double width and double height lines.
            QMatrix textScale;

            if (y < _lineProperties.size()) {
                if ((_lineProperties[y] & LINE_DOUBLEWIDTH) != 0) {
                    textScale.scale(2, 1);
                }

                if ((_lineProperties[y] & LINE_DOUBLEHEIGHT) != 0) {
                    textScale.scale(1, 2);
                }
            }

            //Apply text scaling matrix.
            paint.setWorldTransform(QTransform(textScale), true);

            //calculate the area in which the text will be drawn
            QRect textArea = QRect(_contentRect.left() + contentsRect().left() + _fontWidth * x,
                                   _contentRect.top() + contentsRect().top() + _fontHeight * y,
                                   _fontWidth * len,
                                   _fontHeight);

            //move the calculated area to take account of scaling applied to the painter.
            //the position of the area from the origin (0,0) is scaled
            //by the opposite of whatever
            //transformation has been applied to the painter.  this ensures that
            //painting does actually start from textArea.topLeft()
            //(instead of textArea.topLeft() * painter-scale)
            textArea.moveTopLeft(textScale.inverted().map(textArea.topLeft()));

            QString unistr = QString::fromUcs4(univec.data(), univec.length());

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
            paint.setWorldTransform(QTransform(textScale.inverted()), true);

            if (y < _lineProperties.size() - 1) {
                //double-height _lines are represented by two adjacent _lines
                //containing the same characters
                //both _lines will have the LINE_DOUBLEHEIGHT attribute.
                //If the current line has the LINE_DOUBLEHEIGHT attribute,
                //we can therefore skip the next line
                if ((_lineProperties[y] & LINE_DOUBLEHEIGHT) != 0) {
                    y++;
                }
            }

            x += len - 1;
        }
    }
}

void TerminalDisplay::drawCurrentResultRect(QPainter& painter)
{
    if(_screenWindow->currentResultLine() == -1) {
        return;
    }

    _searchResultRect.setRect(0, _contentRect.top() + (_screenWindow->currentResultLine() - _screenWindow->currentLine()) * _fontHeight,
                              _columns * _fontWidth, _fontHeight);
    painter.fillRect(_searchResultRect, QColor(0, 0, 255, 80));
}

void TerminalDisplay::highlightScrolledLines(QPainter& painter)
{
    if (!_highlightScrolledLinesControl.enabled) {
        return;
    }

    QColor color = QColor(_colorTable[Color4Index]);
    color.setAlpha(_highlightScrolledLinesControl.timer->isActive() ? 255 : 150);
    painter.fillRect(_highlightScrolledLinesControl.rect, color);
}

QRegion TerminalDisplay::highlightScrolledLinesRegion(bool nothingChanged)
{
    QRegion dirtyRegion;
    const int highlightLeftPosition = _scrollbarLocation == Enum::ScrollBarLeft ? _scrollBar->width() : 0;

    int start = 0;
    int nb_lines = abs(_screenWindow->scrollCount());
    if (nb_lines > 0 && _scrollBar->maximum() > 0) {
        QRect new_highlight;
        bool addToCurrentHighlight = _highlightScrolledLinesControl.timer->isActive() &&
                                     (_screenWindow->scrollCount() * _highlightScrolledLinesControl.previousScrollCount > 0);

        if (addToCurrentHighlight) {
            if (_screenWindow->scrollCount() > 0) {
                start = -1 * (_highlightScrolledLinesControl.previousScrollCount + _screenWindow->scrollCount()) + _screenWindow->windowLines();
            } else {
                start = -1 * _highlightScrolledLinesControl.previousScrollCount;
            }
            _highlightScrolledLinesControl.previousScrollCount += _screenWindow->scrollCount();
        } else {
            start = _screenWindow->scrollCount() > 0 ? _screenWindow->windowLines() - nb_lines : 0;
            _highlightScrolledLinesControl.previousScrollCount = _screenWindow->scrollCount();
        }

        new_highlight.setRect(highlightLeftPosition, _contentRect.top() + start * _fontHeight, HIGHLIGHT_SCROLLED_LINES_WIDTH, nb_lines * _fontHeight);
        new_highlight.setTop(std::max(new_highlight.top(), _contentRect.top()));
        new_highlight.setBottom(std::min(new_highlight.bottom(), _contentRect.bottom()));
        if (!new_highlight.isValid()) {
            new_highlight = QRect(0, 0, 0, 0);
        }
        dirtyRegion = new_highlight;

        if (addToCurrentHighlight) {
            _highlightScrolledLinesControl.rect |= new_highlight;
        } else {
            dirtyRegion |= _highlightScrolledLinesControl.rect;
            _highlightScrolledLinesControl.rect = new_highlight;
        }

        _highlightScrolledLinesControl.timer->start();
    } else if (!nothingChanged || _highlightScrolledLinesControl.needToClear) {
        dirtyRegion = _highlightScrolledLinesControl.rect;
        _highlightScrolledLinesControl.rect.setRect(0, 0, 0, 0);
        _highlightScrolledLinesControl.needToClear = false;
    }

    return dirtyRegion;
}

void TerminalDisplay::highlightScrolledLinesEvent()
{
    update(_highlightScrolledLinesControl.rect);
}

QRect TerminalDisplay::imageToWidget(const QRect& imageArea) const
{
    QRect result;
    result.setLeft(_contentRect.left() + _fontWidth * imageArea.left());
    result.setTop(_contentRect.top() + _fontHeight * imageArea.top());
    result.setWidth(_fontWidth * imageArea.width());
    result.setHeight(_fontHeight * imageArea.height());

    return result;
}

QRect TerminalDisplay::widgetToImage(const QRect &widgetArea) const
{
    QRect result;
    result.setLeft(  qMin(_usedColumns - 1, qMax(0, (widgetArea.left()   - contentsRect().left() - _contentRect.left()) / _fontWidth )));
    result.setTop(   qMin(_usedLines   - 1, qMax(0, (widgetArea.top()    - contentsRect().top()  - _contentRect.top() ) / _fontHeight)));
    result.setRight( qMin(_usedColumns - 1, qMax(0, (widgetArea.right()  - contentsRect().left() - _contentRect.left()) / _fontWidth )));
    result.setBottom(qMin(_usedLines   - 1, qMax(0, (widgetArea.bottom() - contentsRect().top()  - _contentRect.top() ) / _fontHeight)));
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

    if (blink && !_blinkCursorTimer->isActive()) {
        _blinkCursorTimer->start();
    }

    if (!blink && _blinkCursorTimer->isActive()) {
        _blinkCursorTimer->stop();
        if (_cursorBlinking) {
            // if cursor is blinking(hidden), blink it again to make it show
            _cursorBlinking = false;
            updateCursor();
        }
        Q_ASSERT(!_cursorBlinking);
    }
}

void TerminalDisplay::setBlinkingTextEnabled(bool blink)
{
    _allowBlinkingText = blink;

    if (blink && !_blinkTextTimer->isActive()) {
        _blinkTextTimer->start();
    }

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
    Q_ASSERT(!_cursorBlinking);

    // if text is blinking (hidden), blink it again to make it shown
    if (_textBlinking) {
        blinkTextEvent();
    }

    // suppress further text blinking
    _blinkTextTimer->stop();
    Q_ASSERT(!_textBlinking);

    _showUrlHint = false;
    FileFilter::HotSpot::stopThumbnailGeneration();
}

void TerminalDisplay::focusInEvent(QFocusEvent*)
{
    if (_allowBlinkingCursor) {
        _blinkCursorTimer->start();
    }

    updateCursor();

    if (_allowBlinkingText && _hasTextBlinker) {
        _blinkTextTimer->start();
    }
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
    if (!isCursorOnDisplay()){
        return;
    }

    const int cursorLocation = loc(cursorPosition().x(), cursorPosition().y());
    Q_ASSERT(cursorLocation < _imageSize);

    int charWidth = _image[cursorLocation].width();
    QRect cursorRect = imageToWidget(QRect(cursorPosition(), QSize(charWidth, 1)));
    update(cursorRect);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                          Geometry & Resizing                              */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::resizeEvent(QResizeEvent *event)
{
    Q_UNUSED(event)

    if (contentsRect().isValid()) {
        // NOTE: This calls setTabText() in TabbedViewContainer::updateTitle(),
        // which might update the widget size again. New resizeEvent
        // won't be called, do not rely on new sizes before this call.
        updateImageSize();
        updateImage();
    }

    const auto scrollBarWidth = _scrollbarLocation != Enum::ScrollBarHidden
                                ? _scrollBar->width()
                                : 0;
    const auto headerHeight = _headerBar->isVisible() ? _headerBar->height() : 0;

    const auto x = width() - scrollBarWidth - _searchBar->width();
    const auto y = headerHeight;
    _searchBar->move(x, y);
}

void TerminalDisplay::propagateSize()
{
    if (_image != nullptr) {
        updateImageSize();
    }
}

void TerminalDisplay::updateImageSize()
{
    Character* oldImage = _image;
    const int oldLines = _lines;
    const int oldColumns = _columns;

    makeImage();

    if (oldImage != nullptr) {
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

    if (!_screenWindow.isNull()) {
        _screenWindow->setWindowLines(_lines);
    }

    _resizing = (oldLines != _lines) || (oldColumns != _columns);

    if (_resizing) {
        showResizeNotification();
        emit changedContentSizeSignal(_contentRect.height(), _contentRect.width()); // expose resizeEvent
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

    _image = new Character[_imageSize];

    clearImage();
}

void TerminalDisplay::clearImage()
{
    for (int i = 0; i < _imageSize; ++i) {
        _image[i] = Screen::DefaultChar;
    }
}

void TerminalDisplay::calcGeometry()
{
    const auto headerHeight = _headerBar->isVisible() ? _headerBar->height() : 0;

    _scrollBar->resize(
        _scrollBar->sizeHint().width(),        // width
        contentsRect().height() - headerHeight // height
    );

    _contentRect = contentsRect().adjusted(_margin + (_highlightScrolledLinesControl.enabled ? HIGHLIGHT_SCROLLED_LINES_WIDTH : 0), _margin,
                                           -_margin - (_highlightScrolledLinesControl.enabled ? HIGHLIGHT_SCROLLED_LINES_WIDTH : 0), -_margin);

    switch (_scrollbarLocation) {
    case Enum::ScrollBarHidden :
        break;
    case Enum::ScrollBarLeft :
        _contentRect.setLeft(_contentRect.left() + _scrollBar->width());
        _scrollBar->move(contentsRect().left(),
                         contentsRect().top() + headerHeight);
        break;
    case Enum::ScrollBarRight:
        _contentRect.setRight(_contentRect.right() - _scrollBar->width());
        _scrollBar->move(contentsRect().left() + contentsRect().width() - _scrollBar->width(),
                         contentsRect().top() + headerHeight);
        break;
    }

    _contentRect.setTop(_contentRect.top() + headerHeight);

    // ensure that display is always at least one column wide
    _columns = qMax(1, _contentRect.width() / _fontWidth);
    _usedColumns = qMin(_usedColumns, _columns);

    // ensure that display is always at least one line high
    _lines = qMax(1, _contentRect.height() / _fontHeight);
    _usedLines = qMin(_usedLines, _lines);

    if(_centerContents) {
        QSize unusedPixels = _contentRect.size() - QSize(_columns * _fontWidth, _lines * _fontHeight);
        _contentRect.adjust(unusedPixels.width() / 2, unusedPixels.height() / 2, 0, 0);
    }
}

// calculate the needed size, this must be synced with calcGeometry()
void TerminalDisplay::setSize(int columns, int lines)
{
    const int scrollBarWidth = _scrollBar->isHidden() ? 0 : _scrollBar->sizeHint().width();
    const int horizontalMargin = _margin * 2;
    const int verticalMargin = _margin * 2;

    QSize newSize = QSize(horizontalMargin + scrollBarWidth + (columns * _fontWidth)  ,
                          verticalMargin + (lines * _fontHeight));

    if (newSize != size()) {
        _size = newSize;
        updateGeometry();
    }
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
    propagateSize();
    emit changedContentSizeSignal(_contentRect.height(), _contentRect.width());
}
void TerminalDisplay::hideEvent(QHideEvent*)
{
    emit changedContentSizeSignal(_contentRect.height(), _contentRect.width());
}

void TerminalDisplay::setMargin(int margin)
{
    if (margin < 0) {
        margin = 0;
    }
    _margin = margin;
    updateImageSize();
}

void TerminalDisplay::setCenterContents(bool enable)
{
    _centerContents = enable;
    calcGeometry();
    update();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Scrollbar                                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::setScrollBarPosition(Enum::ScrollBarPositionEnum position)
{
    if (_scrollbarLocation == position) {
        return;
    }

    _scrollbarLocation = position;
    applyScrollBarPosition(true);
}

void TerminalDisplay::applyScrollBarPosition(bool propagate) {
    _scrollBar->setHidden(_scrollbarLocation == Enum::ScrollBarHidden);

    if (propagate) {
        propagateSize();
        update();
    }
}

void TerminalDisplay::scrollBarPositionChanged(int)
{
    if (_screenWindow.isNull()) {
        return;
    }

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

    disconnect(_scrollBar, &QScrollBar::valueChanged, this, &Konsole::TerminalDisplay::scrollBarPositionChanged);
    _scrollBar->setRange(0, slines - _lines);
    _scrollBar->setSingleStep(1);
    _scrollBar->setPageStep(_lines);
    _scrollBar->setValue(cursor);
    connect(_scrollBar, &QScrollBar::valueChanged, this, &Konsole::TerminalDisplay::scrollBarPositionChanged);
}

void TerminalDisplay::setScrollFullPage(bool fullPage)
{
    _scrollFullPage = fullPage;
}

bool TerminalDisplay::scrollFullPage() const
{
    return _scrollFullPage;
}

void TerminalDisplay::setHighlightScrolledLines(bool highlight)
{
    _highlightScrolledLinesControl.enabled = highlight;

    if (_highlightScrolledLinesControl.enabled && _highlightScrolledLinesControl.timer == nullptr) {
        // setup timer for diming the highlight on scrolled lines
        _highlightScrolledLinesControl.timer = new QTimer(this);
        _highlightScrolledLinesControl.timer->setSingleShot(true);
        _highlightScrolledLinesControl.timer->setInterval(250);
        connect(_highlightScrolledLinesControl.timer, &QTimer::timeout, this, &Konsole::TerminalDisplay::highlightScrolledLinesEvent);
    }
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

    if (!contentsRect().contains(ev->pos())) {
        return;
    }

    if (_screenWindow.isNull()) {
        return;
    }

    // Ignore clicks on the message widget
    if (_readOnlyMessageWidget != nullptr) {
        if (_readOnlyMessageWidget->isVisible() && _readOnlyMessageWidget->frameGeometry().contains(ev->pos())) {
            return;
        }
    }

    if (_outputSuspendedMessageWidget != nullptr) {
        if (_outputSuspendedMessageWidget->isVisible() && _outputSuspendedMessageWidget->frameGeometry().contains(ev->pos())) {
            return;
        }
    }

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn, !_usesMouseTracking);
    QPoint pos = QPoint(charColumn, charLine);

    if (ev->button() == Qt::LeftButton) {
        // request the software keyboard, if any
        if (qApp->autoSipEnabled()) {
            auto behavior = QStyle::RequestSoftwareInputPanel(
                        style()->styleHint(QStyle::SH_RequestSoftwareInputPanel));
            if (hasFocus() || behavior == QStyle::RSIP_OnMouseClick) {
                QEvent event(QEvent::RequestSoftwareInputPanel);
                QApplication::sendEvent(this, &event);
            }
        }

        if (!ev->modifiers()) {
            _lineSelectionMode = false;
            _wordSelectionMode = false;
        }

        // The user clicked inside selected text
        bool selected =  _screenWindow->isSelected(pos.x(), pos.y());

        // Drag only when the Control key is held
        if ((!_ctrlRequiredForDrag || ((ev->modifiers() & Qt::ControlModifier) != 0u)) && selected) {
            _dragInfo.state = diPending;
            _dragInfo.start = ev->pos();
        } else {
            // No reason to ever start a drag event
            _dragInfo.state = diNone;

            _preserveLineBreaks = !(((ev->modifiers() & Qt::ControlModifier) != 0u) && !(ev->modifiers() & Qt::AltModifier));
            _columnSelectionMode = ((ev->modifiers() & Qt::AltModifier) != 0u) && ((ev->modifiers() & Qt::ControlModifier) != 0u);

    // There are a couple of use cases when selecting text :
    // Normal buffer or Alternate buffer when not using Mouse Tracking:
    //  select text or extendSelection or columnSelection or columnSelection + extendSelection
    //
    // Alternate buffer when using Mouse Tracking and with Shift pressed:
    //  select text or columnSelection
            if (!_usesMouseTracking &&
                ((ev->modifiers() == Qt::ShiftModifier) ||
                (((ev->modifiers() & Qt::ShiftModifier) != 0u) && _columnSelectionMode))) {
                    extendSelection(ev->pos());
            } else if ((!_usesMouseTracking && !((ev->modifiers() & Qt::ShiftModifier))) ||
                       (_usesMouseTracking && ((ev->modifiers() & Qt::ShiftModifier) != 0u))) {
                _screenWindow->clearSelection();

                pos.ry() += _scrollBar->value();
                _iPntSel = _pntSel = pos;
                _actSel = 1; // left mouse button pressed but nothing selected yet.
            } else if (_usesMouseTracking && !_readOnly) {
                    emit mouseSignal(0, charColumn + 1, charLine + 1 + _scrollBar->value() - _scrollBar->maximum() , 0);
            }
        }
    } else if (ev->button() == Qt::MidButton) {
        processMidButtonClick(ev);
    } else if (ev->button() == Qt::RightButton) {
        if (!_usesMouseTracking || ((ev->modifiers() & Qt::ShiftModifier) != 0u)) {
            emit configureRequest(ev->pos());
        } else {
            if(!_readOnly) {
                emit mouseSignal(2, charColumn + 1, charLine + 1 + _scrollBar->value() - _scrollBar->maximum() , 0);
            }
        }
    }
}

QSharedPointer<Filter::HotSpot> TerminalDisplay::filterActions(const QPoint& position)
{
    int charLine, charColumn;
    getCharacterPosition(position, charLine, charColumn, false);

    return _filterChain->hotSpotAt(charLine, charColumn);
}

void TerminalDisplay::mouseMoveEvent(QMouseEvent* ev)
{
    int charLine = 0;
    int charColumn = 0;

    getCharacterPosition(ev->pos(), charLine, charColumn, !_usesMouseTracking);

    processFilters();
    // handle filters
    // change link hot-spot appearance on mouse-over
    auto spot = _filterChain->hotSpotAt(charLine, charColumn);
    if ((spot != nullptr) && (spot->type() == Filter::HotSpot::Link || spot->type() == Filter::HotSpot::EMailAddress)) {
        QRegion previousHotspotArea = _mouseOverHotspotArea;
        _mouseOverHotspotArea = QRegion();
        QRect r;
        if (spot->startLine() == spot->endLine()) {
            r.setCoords(spot->startColumn()*_fontWidth + _contentRect.left(),
                        spot->startLine()*_fontHeight + _contentRect.top(),
                        (spot->endColumn())*_fontWidth + _contentRect.left() - 1,
                        (spot->endLine() + 1)*_fontHeight + _contentRect.top() - 1);
            _mouseOverHotspotArea |= r;
        } else {
            r.setCoords(spot->startColumn()*_fontWidth + _contentRect.left(),
                        spot->startLine()*_fontHeight + _contentRect.top(),
                        (_columns)*_fontWidth + _contentRect.left() - 1,
                        (spot->startLine() + 1)*_fontHeight + _contentRect.top() - 1);
            _mouseOverHotspotArea |= r;
            for (int line = spot->startLine() + 1 ; line < spot->endLine() ; line++) {
                r.setCoords(0 * _fontWidth + _contentRect.left(),
                            line * _fontHeight + _contentRect.top(),
                            (_columns)*_fontWidth + _contentRect.left() - 1,
                            (line + 1)*_fontHeight + _contentRect.top() - 1);
                _mouseOverHotspotArea |= r;
            }
            r.setCoords(0 * _fontWidth + _contentRect.left(),
                        spot->endLine()*_fontHeight + _contentRect.top(),
                        (spot->endColumn())*_fontWidth + _contentRect.left() - 1,
                        (spot->endLine() + 1)*_fontHeight + _contentRect.top() - 1);
            _mouseOverHotspotArea |= r;
        }

        if ((_openLinksByDirectClick || ((ev->modifiers() & Qt::ControlModifier) != 0u)) && (cursor().shape() != Qt::PointingHandCursor)) {
            setCursor(Qt::PointingHandCursor);
        }

        /* can't use qobject_cast because moc is broken for inner classes */
        auto fileSpot = spot.dynamicCast<FileFilter::HotSpot>();
        if (fileSpot != _currentlyHoveredHotspot) {
            _currentlyHoveredHotspot = fileSpot;
            if (fileSpot) {
                fileSpot->requestThumbnail(ev->modifiers(), ev->globalPos());
            }
        }

        update(_mouseOverHotspotArea | previousHotspotArea);
    } else if (!_mouseOverHotspotArea.isEmpty()) {
        if ((_openLinksByDirectClick || ((ev->modifiers() & Qt::ControlModifier) != 0u)) || (cursor().shape() == Qt::PointingHandCursor)) {
            setCursor(_usesMouseTracking ? Qt::ArrowCursor : Qt::IBeamCursor);
        }

        update(_mouseOverHotspotArea);
        // set hotspot area to an invalid rectangle
        _mouseOverHotspotArea = QRegion();
        FileFilter::HotSpot::stopThumbnailGeneration();
        _currentlyHoveredHotspot.clear();
    }

    // for auto-hiding the cursor, we need mouseTracking
    if (ev->buttons() == Qt::NoButton) {
        return;
    }

    // if the program running in the terminal is interested in Mouse Tracking
    // evnets then emit a mouse movement signal, unless the shift key is
    // being held down, which overrides this.
    if (_usesMouseTracking && !(ev->modifiers() & Qt::ShiftModifier)) {
        int button = 3;
        if ((ev->buttons() & Qt::LeftButton) != 0u) {
            button = 0;
        }
        if ((ev->buttons() & Qt::MidButton) != 0u) {
            button = 1;
        }
        if ((ev->buttons() & Qt::RightButton) != 0u) {
            button = 2;
        }

        emit mouseSignal(button,
                         charColumn + 1,
                         charLine + 1 + _scrollBar->value() - _scrollBar->maximum(),
                         1);

        return;
    }

    if (_dragInfo.state == diPending) {
        // we had a mouse down, but haven't confirmed a drag yet
        // if the mouse has moved sufficiently, we will confirm

        const int distance = QApplication::startDragDistance();
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

    if (_actSel == 0) {
        return;
    }

// don't extend selection while pasting
    if ((ev->buttons() & Qt::MidButton) != 0u) {
        return;
    }

    extendSelection(ev->pos());
}

void TerminalDisplay::leaveEvent(QEvent *)
{
    // remove underline from an active link when cursor leaves the widget area,
    // also restore regular mouse cursor shape
    if(!_mouseOverHotspotArea.isEmpty()) {
        update(_mouseOverHotspotArea);
        _mouseOverHotspotArea = QRegion();
        setCursor(Qt::IBeamCursor);
    }
}

void TerminalDisplay::extendSelection(const QPoint& position)
{
    if (_screenWindow.isNull()) {
        return;
    }

    //if ( !contentsRect().contains(ev->pos()) ) return;
    const QPoint tL  = contentsRect().topLeft();
    const int    tLx = tL.x();
    const int    tLy = tL.y();
    const int    scroll = _scrollBar->value();

    // we're in the process of moving the mouse with the left button pressed
    // the mouse cursor will kept caught within the bounds of the text in
    // this widget.

    int linesBeyondWidget = 0;

    QRect textBounds(tLx + _contentRect.left(),
                     tLy + _contentRect.top(),
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
    getCharacterPosition(pos, charLine, charColumn, true);

    QPoint here = QPoint(charColumn, charLine);
    QPoint ohere;
    QPoint _iPntSelCorr = _iPntSel;
    _iPntSelCorr.ry() -= _scrollBar->value();
    QPoint _pntSelCorr = _pntSel;
    _pntSelCorr.ry() -= _scrollBar->value();
    bool swapping = false;

    if (_wordSelectionMode) {
        // Extend to word boundaries
        const bool left_not_right = (here.y() < _iPntSelCorr.y() ||
                                     (here.y() == _iPntSelCorr.y() && here.x() < _iPntSelCorr.x()));
        const bool old_left_not_right = (_pntSelCorr.y() < _iPntSelCorr.y() ||
                                         (_pntSelCorr.y() == _iPntSelCorr.y() && _pntSelCorr.x() < _iPntSelCorr.x()));
        swapping = left_not_right != old_left_not_right;

        // Find left (left_not_right ? from here : from start of word)
        QPoint left = left_not_right ? here : _iPntSelCorr;
        // Find left (left_not_right ? from end of word : from here)
        QPoint right = left_not_right ? _iPntSelCorr : here;

        if (left.y() < 0 || left.y() >= _lines || left.x() < 0 || left.x() >= _columns) {
            left = _pntSelCorr;
        } else {
            left = findWordStart(left);
        }
        if (right.y() < 0 || right.y() >= _lines || right.x() < 0 || right.x() >= _columns) {
            right = _pntSelCorr;
        } else {
            right = findWordEnd(right);
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
        if (above_not_below) {
            ohere = findLineEnd(_iPntSelCorr);
            here = findLineStart(here);
        } else {
            ohere = findLineStart(_iPntSelCorr);
            here = findLineEnd(here);
        }

        swapping = !(_tripleSelBegin == ohere);
        _tripleSelBegin = ohere;

        ohere.rx()++;
    }

    int offset = 0;
    if (!_wordSelectionMode && !_lineSelectionMode) {
        const bool left_not_right = (here.y() < _iPntSelCorr.y() ||
                                     (here.y() == _iPntSelCorr.y() && here.x() < _iPntSelCorr.x()));
        const bool old_left_not_right = (_pntSelCorr.y() < _iPntSelCorr.y() ||
                                         (_pntSelCorr.y() == _iPntSelCorr.y() && _pntSelCorr.x() < _iPntSelCorr.x()));
        swapping = left_not_right != old_left_not_right;

        // Find left (left_not_right ? from here : from start)
        const QPoint left = left_not_right ? here : _iPntSelCorr;

        // Find right (left_not_right ? from start : from here)
        QPoint right = left_not_right ? _iPntSelCorr : here;

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

    if ((here == _pntSelCorr) && (scroll == _scrollBar->value())) {
        return; // not moved
    }

    if (here == ohere) {
        return; // It's not left, it's not right.
    }

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
    if (_screenWindow.isNull()) {
        return;
    }

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn, !_usesMouseTracking);

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

            if (_usesMouseTracking && !(ev->modifiers() & Qt::ShiftModifier)) {
                emit mouseSignal(0,
                                 charColumn + 1,
                                 charLine + 1 + _scrollBar->value() - _scrollBar->maximum() , 2);
            }
        }
        _dragInfo.state = diNone;
    }

    if (_usesMouseTracking &&
            (ev->button() == Qt::RightButton || ev->button() == Qt::MidButton) &&
            !(ev->modifiers() & Qt::ShiftModifier)) {
        emit mouseSignal(ev->button() == Qt::MidButton ? 1 : 2,
                         charColumn + 1,
                         charLine + 1 + _scrollBar->value() - _scrollBar->maximum() ,
                         2);
    }

    if (!_screenWindow->screen()->hasSelection() && (_openLinksByDirectClick || ((ev->modifiers() & Qt::ControlModifier) != 0u))) {
        auto spot = _filterChain->hotSpotAt(charLine, charColumn);
        if ((spot != nullptr) && (spot->type() == Filter::HotSpot::Link || spot->type() == Filter::HotSpot::EMailAddress)) {
            spot->activate();
        }
    }
}

void TerminalDisplay::getCharacterPosition(const QPoint& widgetPoint, int& line, int& column, bool edge) const
{
    // the column value returned can be equal to _usedColumns (when edge == true),
    // which is the position just after the last character displayed in a line.
    //
    // this is required so that the user can select characters in the right-most
    // column (or left-most for right-to-left input)
    const int columnMax = edge ? _usedColumns : _usedColumns - 1;
    const int xOffset = edge ? _fontWidth / 2 : 0;
    column = qBound(0, (widgetPoint.x() + xOffset - contentsRect().left() - _contentRect.left()) / _fontWidth, columnMax);
    line = qBound(0, (widgetPoint.y() - contentsRect().top() - _contentRect.top()) / _fontHeight, _usedLines - 1);
}

void TerminalDisplay::setExpandedMode(bool expand)
{
    _headerBar->setExpandedMode(expand);
}

void TerminalDisplay::updateLineProperties()
{
    if (_screenWindow.isNull()) {
        return;
    }

    _lineProperties = _screenWindow->getLineProperties();
}

void TerminalDisplay::processMidButtonClick(QMouseEvent* ev)
{
    if (!_usesMouseTracking || ((ev->modifiers() & Qt::ShiftModifier) != 0u)) {
        const bool appendEnter = (ev->modifiers() & Qt::ControlModifier) != 0u;

        if (_middleClickPasteMode == Enum::PasteFromX11Selection) {
            pasteFromX11Selection(appendEnter);
        } else if (_middleClickPasteMode == Enum::PasteFromClipboard) {
            pasteFromClipboard(appendEnter);
        } else {
            Q_ASSERT(false);
        }
    } else {
        if(!_readOnly) {
            int charLine = 0;
            int charColumn = 0;
            getCharacterPosition(ev->pos(), charLine, charColumn, !_usesMouseTracking);

            emit mouseSignal(1, charColumn + 1, charLine + 1 + _scrollBar->value() - _scrollBar->maximum() , 0);
        }
    }
}

void TerminalDisplay::mouseDoubleClickEvent(QMouseEvent* ev)
{
    // Yes, successive middle click can trigger this event
    if (ev->button() == Qt::MidButton) {
        processMidButtonClick(ev);
        return;
    }

    if (ev->button() != Qt::LeftButton) {
        return;
    }
    if (_screenWindow.isNull()) {
        return;
    }

    int charLine = 0;
    int charColumn = 0;

    getCharacterPosition(ev->pos(), charLine, charColumn, !_usesMouseTracking);

    QPoint pos(qMin(charColumn, _columns - 1), qMin(charLine, _lines - 1));

    // pass on double click as two clicks.
    if (_usesMouseTracking && !(ev->modifiers() & Qt::ShiftModifier)) {
        if(!_readOnly) {
            // Send just _ONE_ click event, since the first click of the double click
            // was already sent by the click handler
            emit mouseSignal(0, charColumn + 1,
                             charLine + 1 + _scrollBar->value() - _scrollBar->maximum(),
                             0);  // left button
        }
        return;
    }

    _screenWindow->clearSelection();
    _iPntSel = pos;
    _iPntSel.ry() += _scrollBar->value();

    _wordSelectionMode = true;
    _actSel = 2; // within selection

    // find word boundaries...
    {
        // find the start of the word
        const QPoint bgnSel = findWordStart(pos);
        const QPoint endSel = findWordEnd(pos);

        _actSel = 2; // within selection

        _screenWindow->setSelectionStart(bgnSel.x() , bgnSel.y() , false);
        _screenWindow->setSelectionEnd(endSel.x() , endSel.y());

        copyToX11Selection();
    }

    _possibleTripleClick = true;

    QTimer::singleShot(QApplication::doubleClickInterval(), this, [this]() {
        _possibleTripleClick = false;
    });
}

void TerminalDisplay::wheelEvent(QWheelEvent* ev)
{
    // Only vertical scrolling is supported
    if (qAbs(ev->angleDelta().y()) < qAbs(ev->angleDelta().x())) {
        return;
    }

    const int modifiers = ev->modifiers();

    // ctrl+<wheel> for zooming, like in konqueror and firefox
    if (((modifiers & Qt::ControlModifier) != 0u) && _mouseWheelZoom) {
        _scrollWheelState.addWheelEvent(ev);

        int steps = _scrollWheelState.consumeLegacySteps(ScrollState::DEFAULT_ANGLE_SCROLL_LINE);
        for (;steps > 0; --steps) {
            // wheel-up for increasing font size
            increaseFontSize();
        }
        for (;steps < 0; ++steps) {
            // wheel-down for decreasing font size
            decreaseFontSize();
        }
    } else if (!_usesMouseTracking && (_scrollBar->maximum() > 0)) {
        // If the program running in the terminal is not interested in Mouse
        // Tracking events, send the event to the scrollbar if the slider
        // has room to move

        _scrollWheelState.addWheelEvent(ev);

        _scrollBar->event(ev);

        // Reapply scrollbar position since the scrollbar event handler
        // sometimes makes the scrollbar visible when set to hidden.
        // Don't call propagateSize and update, since nothing changed.
        applyScrollBarPosition(false);

        Q_ASSERT(_sessionController != nullptr);

        _sessionController->setSearchStartToWindowCurrentLine();
        _scrollWheelState.clearAll();
    } else if (!_readOnly) {
        _scrollWheelState.addWheelEvent(ev);

        Q_ASSERT(!_sessionController->session().isNull());

        if(!_usesMouseTracking && !_sessionController->session()->isPrimaryScreen() && _alternateScrolling) {
            // Send simulated up / down key presses to the terminal program
            // for the benefit of programs such as 'less' (which use the alternate screen)

            // assume that each Up / Down key event will cause the terminal application
            // to scroll by one line.
            //
            // to get a reasonable scrolling speed, scroll by one line for every 5 degrees
            // of mouse wheel rotation.  Mouse wheels typically move in steps of 15 degrees,
            // giving a scroll of 3 lines

            const int lines = _scrollWheelState.consumeSteps(static_cast<int>(_fontHeight * qApp->devicePixelRatio()), ScrollState::degreesToAngle(5));
            const int keyCode = lines > 0 ? Qt::Key_Up : Qt::Key_Down;
            QKeyEvent keyEvent(QEvent::KeyPress, keyCode, Qt::NoModifier);

            for (int i = 0; i < abs(lines); i++) {
                _screenWindow->screen()->setCurrentTerminalDisplay(this);
                emit keyPressedSignal(&keyEvent);
            }
        } else if (_usesMouseTracking) {
            // terminal program wants notification of mouse activity

            int charLine;
            int charColumn;
#if (QT_VERSION >= QT_VERSION_CHECK(5, 14, 0))
            getCharacterPosition(ev->position().toPoint() , charLine , charColumn, !_usesMouseTracking);
#else
            getCharacterPosition(ev->pos() , charLine , charColumn, !_usesMouseTracking);
#endif
            const int steps = _scrollWheelState.consumeLegacySteps(ScrollState::DEFAULT_ANGLE_SCROLL_LINE);
            const int button = (steps > 0) ? 4 : 5;
            for (int i = 0; i < abs(steps); ++i) {
                emit mouseSignal(button,
                                 charColumn + 1,
                                 charLine + 1 + _scrollBar->value() - _scrollBar->maximum() ,
                                 0);
            }
        }
    }
}

void TerminalDisplay::viewScrolledByUser()
{
    Q_ASSERT(_sessionController != nullptr);
    _sessionController->setSearchStartToWindowCurrentLine();
}

/* Moving left/up from the line containing pnt, return the starting
   offset point which the given line is continuously wrapped
   (top left corner = 0,0; previous line not visible = 0,-1).
*/
QPoint TerminalDisplay::findLineStart(const QPoint &pnt)
{
    const int visibleScreenLines = _lineProperties.size();
    const int topVisibleLine = _screenWindow->currentLine();
    Screen *screen = _screenWindow->screen();
    int line = pnt.y();
    int lineInHistory= line + topVisibleLine;

    QVector<LineProperty> lineProperties = _lineProperties;

    while (lineInHistory > 0) {
        for (; line > 0; line--, lineInHistory--) {
            // Does previous line wrap around?
            if ((lineProperties[line - 1] & LINE_WRAPPED) == 0) {
                return {0, lineInHistory - topVisibleLine};
            }
        }

        if (lineInHistory < 1) {
            break;
        }

        // _lineProperties is only for the visible screen, so grab new data
        int newRegionStart = qMax(0, lineInHistory - visibleScreenLines);
        lineProperties = screen->getLineProperties(newRegionStart, lineInHistory - 1);
        line = lineInHistory - newRegionStart;
    }
    return {0, lineInHistory - topVisibleLine};
}

/* Moving right/down from the line containing pnt, return the ending
   offset point which the given line is continuously wrapped.
*/
QPoint TerminalDisplay::findLineEnd(const QPoint &pnt)
{
    const int visibleScreenLines = _lineProperties.size();
    const int topVisibleLine = _screenWindow->currentLine();
    const int maxY = _screenWindow->lineCount() - 1;
    Screen *screen = _screenWindow->screen();
    int line = pnt.y();
    int lineInHistory= line + topVisibleLine;

    QVector<LineProperty> lineProperties = _lineProperties;

    while (lineInHistory < maxY) {
        for (; line < lineProperties.count() && lineInHistory < maxY; line++, lineInHistory++) {
            // Does current line wrap around?
            if ((lineProperties[line] & LINE_WRAPPED) == 0) {
                return {_columns - 1, lineInHistory - topVisibleLine};
            }
        }

        line = 0;
        lineProperties = screen->getLineProperties(lineInHistory, qMin(lineInHistory + visibleScreenLines, maxY));
    }
    return {_columns - 1, lineInHistory - topVisibleLine};
}

QPoint TerminalDisplay::findWordStart(const QPoint &pnt)
{
    const int regSize = qMax(_screenWindow->windowLines(), 10);
    const int firstVisibleLine = _screenWindow->currentLine();

    Screen *screen = _screenWindow->screen();
    Character *image = _image;
    Character *tmp_image = nullptr;

    int imgLine = pnt.y();
    int x = pnt.x();
    int y = imgLine + firstVisibleLine;
    int imgLoc = loc(x, imgLine);
    QVector<LineProperty> lineProperties = _lineProperties;
    const QChar selClass = charClass(image[imgLoc]);
    const int imageSize = regSize * _columns;

    while (true) {
        for (;;imgLoc--, x--) {
            if (imgLoc < 1) {
                // no more chars in this region
                break;
            }
            if (x > 0) {
                // has previous char on this line
                if (charClass(image[imgLoc - 1]) == selClass) {
                    continue;
                }
                goto out;
            } else if (imgLine > 0) {
                // not the first line in the session
                if ((lineProperties[imgLine - 1] & LINE_WRAPPED) != 0) {
                    // have continuation on prev line
                    if (charClass(image[imgLoc - 1]) == selClass) {
                        x = _columns;
                        imgLine--;
                        y--;
                        continue;
                    }
                }
                goto out;
            } else if (y > 0) {
                // want more data, but need to fetch new region
                break;
            } else {
                goto out;
            }
        }
        if (y <= 0) {
            // No more data
            goto out;
        }
        int newRegStart = qMax(0, y - regSize + 1);
        lineProperties = screen->getLineProperties(newRegStart, y - 1);
        imgLine = y - newRegStart;

        delete[] tmp_image;
        tmp_image = new Character[imageSize];
        image = tmp_image;

        screen->getImage(tmp_image, imageSize, newRegStart, y - 1);
        imgLoc = loc(x, imgLine);
        if (imgLoc < 1) {
            // Reached the start of the session
            break;
        }
    }
out:
    delete[] tmp_image;
    return {x, y - firstVisibleLine};
}

QPoint TerminalDisplay::findWordEnd(const QPoint &pnt)
{
    const int regSize = qMax(_screenWindow->windowLines(), 10);
    const int curLine = _screenWindow->currentLine();
    int i = pnt.y();
    int x = pnt.x();
    int y = i + curLine;
    int j = loc(x, i);
    QVector<LineProperty> lineProperties = _lineProperties;
    Screen *screen = _screenWindow->screen();
    Character *image = _image;
    Character *tmp_image = nullptr;
    const QChar selClass = charClass(image[j]);
    const int imageSize = regSize * _columns;
    const int maxY = _screenWindow->lineCount() - 1;
    const int maxX = _columns - 1;

    while (true) {
        const int lineCount = lineProperties.count();
        for (;;j++, x++) {
            if (x < maxX) {
                if (charClass(image[j + 1]) == selClass &&
                    // A colon right before whitespace is never part of a word
                    ! (image[j + 1].character == ':' && charClass(image[j + 2]) == QLatin1Char(' '))) {
                    continue;
                }
                goto out;
            } else if (i < lineCount - 1) {
                if (((lineProperties[i] & LINE_WRAPPED) != 0) &&
                    charClass(image[j + 1]) == selClass &&
                    // A colon right before whitespace is never part of a word
                    ! (image[j + 1].character == ':' && charClass(image[j + 2]) == QLatin1Char(' '))) {
                    x = -1;
                    i++;
                    y++;
                    continue;
                }
                goto out;
            } else if (y < maxY) {
                if (i < lineCount && ((lineProperties[i] & LINE_WRAPPED) == 0)) {
                    goto out;
                }
                break;
            } else {
                goto out;
            }
        }
        int newRegEnd = qMin(y + regSize - 1, maxY);
        lineProperties = screen->getLineProperties(y, newRegEnd);
        i = 0;
        if (tmp_image == nullptr) {
            tmp_image = new Character[imageSize];
            image = tmp_image;
        }
        screen->getImage(tmp_image, imageSize, y, newRegEnd);
        x--;
        j = loc(x, i);
    }
out:
    y -= curLine;
    // In word selection mode don't select @ (64) if at end of word.
    if (((image[j].rendition & RE_EXTENDED_CHAR) == 0) &&
        (QChar(image[j].character) == QLatin1Char('@')) &&
        (y > pnt.y() || x > pnt.x())) {
        if (x > 0) {
            x--;
        } else {
            y--;
        }
    }
    delete[] tmp_image;

    return {x, y};
}

Screen::DecodingOptions TerminalDisplay::currentDecodingOptions()
{
    Screen::DecodingOptions decodingOptions;
    if (_preserveLineBreaks) {
        decodingOptions |= Screen::PreserveLineBreaks;
    }
    if (_trimLeadingSpaces) {
        decodingOptions |= Screen::TrimLeadingWhitespace;
    }
    if (_trimTrailingSpaces) {
        decodingOptions |= Screen::TrimTrailingWhitespace;
    }

    return decodingOptions;
}

void TerminalDisplay::mouseTripleClickEvent(QMouseEvent* ev)
{
    if (_screenWindow.isNull()) {
        return;
    }

    int charLine;
    int charColumn;
    getCharacterPosition(ev->pos(), charLine, charColumn, true);
    selectLine(QPoint(charColumn, charLine),
               _tripleClickMode == Enum::SelectWholeLine);
}

void TerminalDisplay::selectLine(QPoint pos, bool entireLine)
{
    _iPntSel = pos;

    _screenWindow->clearSelection();

    _lineSelectionMode = true;
    _wordSelectionMode = false;

    _actSel = 2; // within selection

    if (!entireLine) { // Select from cursor to end of line
        _tripleSelBegin = findWordStart(_iPntSel);
        _screenWindow->setSelectionStart(_tripleSelBegin.x(),
                                         _tripleSelBegin.y() , false);
    } else {
        _tripleSelBegin = findLineStart(_iPntSel);
        _screenWindow->setSelectionStart(0 , _tripleSelBegin.y() , false);
    }

    _iPntSel = findLineEnd(_iPntSel);
    _screenWindow->setSelectionEnd(_iPntSel.x() , _iPntSel.y());

    copyToX11Selection();

    _iPntSel.ry() += _scrollBar->value();
}

void TerminalDisplay::selectCurrentLine()
{
    if (_screenWindow.isNull()) {
        return;
    }

    selectLine(cursorPosition(), true);
}

void TerminalDisplay::selectAll()
{
    if (_screenWindow.isNull()) {
        return;
    }

    _preserveLineBreaks = true;
    _screenWindow->setSelectionByLineRange(0, _screenWindow->lineCount());
    copyToX11Selection();
}

bool TerminalDisplay::focusNextPrevChild(bool next)
{
    // for 'Tab', always disable focus switching among widgets
    // for 'Shift+Tab', leave the decision to higher level
    if (next) {
        return false;
    } else {
        return QWidget::focusNextPrevChild(next);
    }
}

QChar TerminalDisplay::charClass(const Character& ch) const
{
    if ((ch.rendition & RE_EXTENDED_CHAR) != 0) {
        ushort extendedCharLength = 0;
        const uint* chars = ExtendedCharTable::instance.lookupExtendedChar(ch.character, extendedCharLength);
        if ((chars != nullptr) && extendedCharLength > 0) {
            const QString s = QString::fromUcs4(chars, extendedCharLength);
            if (_wordCharacters.contains(s, Qt::CaseInsensitive)) {
                return QLatin1Char('a');
            }
            bool letterOrNumber = false;
            for (int i = 0; !letterOrNumber && i < s.size(); ++i) {
                letterOrNumber = s.at(i).isLetterOrNumber();
            }
            return letterOrNumber ? QLatin1Char('a') : s.at(0);
        }
        return 0;
    } else {
        const QChar qch(ch.character);
        if (qch.isSpace()) {
            return QLatin1Char(' ');
        }

        if (qch.isLetterOrNumber() || _wordCharacters.contains(qch, Qt::CaseInsensitive)) {
            return QLatin1Char('a');
        }

        return qch;
    }
}

void TerminalDisplay::setWordCharacters(const QString& wc)
{
    _wordCharacters = wc;
}

void TerminalDisplay::setUsesMouseTracking(bool on)
{
    _usesMouseTracking = on;
    setCursor(_usesMouseTracking ? Qt::ArrowCursor : Qt::IBeamCursor);
}
bool TerminalDisplay::usesMouseTracking() const
{
    return _usesMouseTracking;
}

void TerminalDisplay::setAlternateScrolling(bool enable)
{
    _alternateScrolling = enable;
}
bool TerminalDisplay::alternateScrolling() const
{
    return _alternateScrolling;
}

void TerminalDisplay::setBracketedPasteMode(bool on)
{
    _bracketedPasteMode = on;
}
bool TerminalDisplay::bracketedPasteMode() const
{
    return _bracketedPasteMode;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               Clipboard                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void TerminalDisplay::doPaste(QString text, bool appendReturn)
{
    if (_screenWindow.isNull()) {
        return;
    }

    if (_readOnly) {
        return;
    }

    if (appendReturn) {
        text.append(QLatin1String("\r"));
    }

    if (text.length() > 8000) {
        if (KMessageBox::warningContinueCancel(window(),
                        i18np("Are you sure you want to paste %1 character?",
                              "Are you sure you want to paste %1 characters?",
                              text.length()),
                        i18n("Confirm Paste"),
                        KStandardGuiItem::cont(),
                        KStandardGuiItem::cancel(),
                        QStringLiteral("ShowPasteHugeTextWarning")) == KMessageBox::Cancel) {
            return;
        }
    }

    // Most code in Konsole uses UTF-32. We're filtering
    // UTF-16 here, as all control characters can be represented
    // in this encoding as single code unit. If you ever need to
    // filter anything above 0xFFFF (specific code points or
    // categories which contain such code points), convert text to
    // UTF-32 using QString::toUcs4() and use QChar static
    // methods which take "uint ucs4".
    static const QVector<ushort> whitelist = { u'\t', u'\r', u'\n' };
    static const auto isUnsafe = [](const QChar &c) {
        return (c.category() == QChar::Category::Other_Control && !whitelist.contains(c.unicode()));
    };
    // Returns control sequence string (e.g. "^C") for control character c
    static const auto charToSequence = [](const QChar &c) {
        if (c.unicode() <= 0x1F) {
            return QStringLiteral("^%1").arg(QChar(u'@' + c.unicode()));
        } else if (c.unicode() == 0x7F) {
            return QStringLiteral("^?");
        } else if (c.unicode() >= 0x80 && c.unicode() <= 0x9F){
            return QStringLiteral("^[%1").arg(QChar(u'@' + c.unicode() - 0x80));
        }
        return QString();
    };

    const QMap<ushort, QString> characterDescriptions = {
        {0x0003, i18n("End Of Text/Interrupt: may exit the current process")},
        {0x0004, i18n("End Of Transmission: may exit the current process")},
        {0x0007, i18n("Bell: will try to emit an audible warning")},
        {0x0008, i18n("Backspace")},
        {0x0013, i18n("Device Control Three/XOFF: suspends output")},
        {0x001a, i18n("Substitute/Suspend: may suspend current process")},
        {0x001b, i18n("Escape: used for manipulating terminal state")},
        {0x001c, i18n("File Separator/Quit: may abort the current process")},
    };

    QStringList unsafeCharacters;
    for (const QChar &c : text) {
        if (isUnsafe(c)) {
            const QString sequence = charToSequence(c);
            const QString description = characterDescriptions.value(c.unicode(), QString());
            QString entry = QStringLiteral("U+%1").arg(c.unicode(), 4, 16, QLatin1Char('0'));
            if(!sequence.isEmpty()) {
                entry += QStringLiteral("\t%1").arg(sequence);
            }
            if(!description.isEmpty()) {
                entry += QStringLiteral("\t%1").arg(description);
            }
            unsafeCharacters.append(entry);
        }
    }
    unsafeCharacters.removeDuplicates();

    if (!unsafeCharacters.isEmpty()) {
        int result = KMessageBox::warningYesNoCancelList(window(),
                i18n("The text you're trying to paste contains hidden control characters, "
                    "do you want to filter them out?"),
                unsafeCharacters,
                i18nc("@title", "Confirm Paste"),
                KGuiItem(i18nc("@action:button",
                    "Paste &without control characters"),
                    QStringLiteral("filter-symbolic")),
                KGuiItem(i18nc("@action:button",
                    "&Paste everything"),
                    QStringLiteral("edit-paste")),
                KGuiItem(i18nc("@action:button",
                    "&Cancel"),
                    QStringLiteral("dialog-cancel")),
                QStringLiteral("ShowPasteUnprintableWarning")
            );
        switch(result){
        case KMessageBox::Cancel:
            return;
        case KMessageBox::Yes: {
            QString sanitized;
            for (const QChar &c : text) {
                if (!isUnsafe(c)) {
                    sanitized.append(c);
                }
            }
            text = sanitized;
        }
        case KMessageBox::No:
            break;
        default:
            break;
        }
    }

    if (!text.isEmpty()) {
        // replace CRLF with CR first, fixes issues with pasting multiline
        // text from gtk apps (e.g. Firefox), bug 421480
        text.replace(QLatin1String("\r\n"), QLatin1String("\r"));

        text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
        if (bracketedPasteMode()) {
            text.remove(QLatin1String("\033"));
            text.prepend(QLatin1String("\033[200~"));
            text.append(QLatin1String("\033[201~"));
        }
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

void TerminalDisplay::setCopyTextAsHTML(bool enabled)
{
    _copyTextAsHTML = enabled;
}

void TerminalDisplay::copyToX11Selection()
{
    if (_screenWindow.isNull()) {
        return;
    }


    const QString &text = _screenWindow->selectedText(currentDecodingOptions());
    if (text.isEmpty()) {
        return;
    }

    auto mimeData = new QMimeData;
    mimeData->setText(text);

    if (_copyTextAsHTML) {
        mimeData->setHtml(_screenWindow->selectedText(currentDecodingOptions() | Screen::ConvertToHtml));
    }

    if (QApplication::clipboard()->supportsSelection()) {
        QApplication::clipboard()->setMimeData(mimeData, QClipboard::Selection);
    }

    if (_autoCopySelectedText) {
        QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);
    }
}

void TerminalDisplay::copyToClipboard()
{
    if (_screenWindow.isNull()) {
        return;
    }

    const QString &text = _screenWindow->selectedText(currentDecodingOptions());
    if (text.isEmpty()) {
        return;
    }

    auto mimeData = new QMimeData;
    mimeData->setText(text);

    if (_copyTextAsHTML) {
        mimeData->setHtml(_screenWindow->selectedText(currentDecodingOptions() | Screen::ConvertToHtml));
    }

    QApplication::clipboard()->setMimeData(mimeData, QClipboard::Clipboard);
}

void TerminalDisplay::pasteFromClipboard(bool appendEnter)
{
    QString text;
    const QMimeData *mimeData = QApplication::clipboard()->mimeData(QClipboard::Clipboard);

    // When pasting urls of local files:
    // - remove the scheme part, "file://"
    // - paste the path(s) as a space-separated list of strings, which are quoted if needed
    if (!mimeData->hasUrls()) { // fast path if there are no urls
        text = mimeData->text();
    } else { // handle local file urls
        const QList<QUrl> list = mimeData->urls();
        for (const QUrl &url : list) {
            if (url.isLocalFile()) {
                text += KShell::quoteArg(url.toLocalFile());
                text += QLatin1Char(' ');
            } else { // can users copy urls of both local and remote files at the same time?
                text = mimeData->text();
                break;
            }
        }
    }

    doPaste(text, appendEnter);
}

void TerminalDisplay::pasteFromX11Selection(bool appendEnter)
{
    if (QApplication::clipboard()->supportsSelection()) {
        QString text = QApplication::clipboard()->text(QClipboard::Selection);
        doPaste(text, appendEnter);
    }
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

    if (!_readOnly && isCursorOnDisplay()) {
        _inputMethodData.preeditString = event->preeditString();
        update(preeditRect() | _inputMethodData.previousPreeditRect);
    }
    event->accept();
}

QVariant TerminalDisplay::inputMethodQuery(Qt::InputMethodQuery query) const
{
    const QPoint cursorPos = cursorPosition();
    switch (query) {
    case Qt::ImCursorRectangle:
        return imageToWidget(QRect(cursorPos.x(), cursorPos.y(), 1, 1));
    case Qt::ImFont:
        return font();
    case Qt::ImCursorPosition:
        // return the cursor position within the current line
        return cursorPos.x();
    case Qt::ImSurroundingText: {
        // return the text from the current line
        QString lineText;
        QTextStream stream(&lineText);
        PlainTextDecoder decoder;
        decoder.begin(&stream);
        if (isCursorOnDisplay()) {
            decoder.decodeLine(&_image[loc(0, cursorPos.y())], _usedColumns, LINE_DEFAULT);
        }
        decoder.end();
        return lineText;
    }
    case Qt::ImCurrentSelection:
        return QString();
    default:
        break;
    }

    return QVariant();
}

QRect TerminalDisplay::preeditRect() const
{
    const int preeditLength = Character::stringWidth(_inputMethodData.preeditString);

    if (preeditLength == 0) {
        return {};
    }
    const QRect stringRect(_contentRect.left() + _fontWidth * cursorPosition().x(),
                           _contentRect.top() + _fontHeight * cursorPosition().y(),
                           _fontWidth * preeditLength,
                           _fontHeight);

    return stringRect.intersected(_contentRect);
}

void TerminalDisplay::drawInputMethodPreeditString(QPainter& painter , const QRect& rect)
{
    if (_inputMethodData.preeditString.isEmpty() || !isCursorOnDisplay()) {
        return;
    }

    const QPoint cursorPos = cursorPosition();

    QColor characterColor;
    const QColor background = _colorTable[DEFAULT_BACK_COLOR];
    const QColor foreground = _colorTable[DEFAULT_FORE_COLOR];
    const Character* style = &_image[loc(cursorPos.x(), cursorPos.y())];

    drawBackground(painter, rect, background, true);
    drawCursor(painter, rect, foreground, background, characterColor);
    drawCharacters(painter, rect, _inputMethodData.preeditString, style, characterColor);

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
    if (!enable) {
        outputSuspended(false);
    }
}

void TerminalDisplay::outputSuspended(bool suspended)
{
    //create the label when this function is first called
    if (_outputSuspendedMessageWidget == nullptr) {
        //This label includes a link to an English language website
        //describing the 'flow control' (Xon/Xoff) feature found in almost
        //all terminal emulators.
        //If there isn't a suitable article available in the target language the link
        //can simply be removed.
        _outputSuspendedMessageWidget = createMessageWidget(i18n("<qt>Output has been "
                                                    "<a href=\"https://en.wikipedia.org/wiki/Software_flow_control\">suspended</a>"
                                                    " by pressing Ctrl+S."
                                                    " Press <b>Ctrl+Q</b> to resume.</qt>"));

        connect(_outputSuspendedMessageWidget, &KMessageWidget::linkActivated, this, [](const QString &url) {
            QDesktopServices::openUrl(QUrl(url));
        });

        _outputSuspendedMessageWidget->setMessageType(KMessageWidget::Warning);
    }

    suspended ? _outputSuspendedMessageWidget->animatedShow() : _outputSuspendedMessageWidget->animatedHide();
}

void TerminalDisplay::dismissOutputSuspendedMessage()
{
    outputSuspended(false);
}

KMessageWidget* TerminalDisplay::createMessageWidget(const QString &text) {
    auto widget = new KMessageWidget(text);
    widget->setWordWrap(true);
    widget->setFocusProxy(this);
    widget->setCursor(Qt::ArrowCursor);

    _verticalLayout->insertWidget(1, widget);

    _searchBar->raise();

    return widget;
}

void TerminalDisplay::updateReadOnlyState(bool readonly) {
    if (_readOnly == readonly) {
        return;
    }

    if (readonly) {
        // Lazy create the readonly messagewidget
        if (_readOnlyMessageWidget == nullptr) {
            _readOnlyMessageWidget = createMessageWidget(i18n("This terminal is read-only."));
            _readOnlyMessageWidget->setIcon(QIcon::fromTheme(QStringLiteral("object-locked")));
        }
    }

    if (_readOnlyMessageWidget != nullptr) {
        readonly ? _readOnlyMessageWidget->animatedShow() : _readOnlyMessageWidget->animatedHide();
    }

    _readOnly = readonly;
}

void TerminalDisplay::scrollScreenWindow(enum ScreenWindow::RelativeScrollMode mode, int amount)
{
    _screenWindow->scrollBy(mode, amount, _scrollFullPage);
    _screenWindow->setTrackOutput(_screenWindow->atEndOfOutput());
    updateLineProperties();
    updateImage();
    viewScrolledByUser();
}

void TerminalDisplay::keyPressEvent(QKeyEvent* event)
{
    if ((_urlHintsModifiers != 0u) && event->modifiers() == _urlHintsModifiers) {
        QList<QSharedPointer<Filter::HotSpot>> hotspots;
        for (const auto &spot : _filterChain->hotSpots()) {
            if (spot->type() == Filter::HotSpot::Link) {
                hotspots.append(spot);
            }
        }
        int nHotSpots = hotspots.count();
        int hintSelected = event->key() - 0x31;
        if (hintSelected >= 0 && hintSelected < 10 && hintSelected < nHotSpots) {
            if (_reverseUrlHints) {
                hintSelected = nHotSpots - hintSelected - 1;
            }
            hotspots.at(hintSelected)->activate();
            _showUrlHint = false;
            update();
            return;
        }

        if (!_showUrlHint) {
            processFilters();
            _showUrlHint = true;
            update();
        }
    }

    if (_currentlyHoveredHotspot) {
        auto fileHotspot = _currentlyHoveredHotspot.dynamicCast<FileFilter::HotSpot>();
        if (!fileHotspot) {
            return;
        }
        fileHotspot->requestThumbnail(event->modifiers(), QCursor::pos());
    }

    _screenWindow->screen()->setCurrentTerminalDisplay(this);

    if (!_readOnly) {
        _actSel = 0; // Key stroke implies a screen update, so TerminalDisplay won't
                     // know where the current selection is.

        if (_allowBlinkingCursor) {
            _blinkCursorTimer->start();
            if (_cursorBlinking) {
                // if cursor is blinking(hidden), blink it again to show it
                blinkCursorEvent();
            }
            Q_ASSERT(!_cursorBlinking);
        }
    }

    if (_searchBar->isVisible() && event->key() == Qt::Key_Escape) {
        _sessionController->searchClosed();
    }

    emit keyPressedSignal(event);

#ifndef QT_NO_ACCESSIBILITY
    if (!_readOnly) {
        QAccessibleTextCursorEvent textCursorEvent(this, _usedColumns * screenWindow()->screen()->getCursorY() + screenWindow()->screen()->getCursorX());
        QAccessible::updateAccessibility(&textCursorEvent);
    }
#endif

    event->accept();
}

void TerminalDisplay::keyReleaseEvent(QKeyEvent *event)
{
    if (_showUrlHint) {
        _showUrlHint = false;
        update();
    }

    if (_readOnly) {
        event->accept();
        return;
    }

    QWidget::keyReleaseEvent(event);
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
            if ((modifiers & currentModifier) != 0u) {
                modifierCount++;
            }
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
        onColorsChanged();
        break;
    case QEvent::FocusOut:
    case QEvent::FocusIn:
        if(_screenWindow != nullptr) {
            // force a redraw on focusIn, fixes the
            // black screen bug when the view is focused
            // but doesn't redraws.
            _screenWindow->notifyOutputChanged();
        }
        update();
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

void TerminalDisplay::bell(const QString& message)
{
    if (_bellMasked) {
        return;
    }

    switch (_bellMode) {
    case Enum::SystemBeepBell:
        KNotification::beep();
        break;
    case Enum::NotifyBell:
        // STABLE API:
        //     Please note that these event names, "BellVisible" and "BellInvisible",
        //     should not change and should be kept stable, because other applications
        //     that use this code via KPart rely on these names for notifications.
        KNotification::event(hasFocus() ? QStringLiteral("BellVisible") : QStringLiteral("BellInvisible"),
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
    QTimer::singleShot(500, this, [this]() {
        _bellMasked = false;
    });
}

void TerminalDisplay::visualBell()
{
    swapFGBGColors();
    QTimer::singleShot(200, this, &Konsole::TerminalDisplay::swapFGBGColors);
}

void TerminalDisplay::swapFGBGColors()
{
    // swap the default foreground & background color
    ColorEntry color = _colorTable[DEFAULT_BACK_COLOR];
    _colorTable[DEFAULT_BACK_COLOR] = _colorTable[DEFAULT_FORE_COLOR];
    _colorTable[DEFAULT_FORE_COLOR] = color;

    onColorsChanged();
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
    const auto mimeData = event->mimeData();
    if ((!_readOnly) && (mimeData != nullptr)
            && (mimeData->hasFormat(QStringLiteral("text/plain"))
                || mimeData->hasFormat(QStringLiteral("text/uri-list")))) {
        event->acceptProposedAction();
    }
}

namespace {
QString extractDroppedText(const QList<QUrl>& urls) {
    QString dropText;
    for (int i = 0 ; i < urls.count() ; i++) {
        KIO::StatJob* job = KIO::mostLocalUrl(urls[i], KIO::HideProgressInfo);
        if (!job->exec()) {
            continue;
        }

        const QUrl url = job->mostLocalUrl();
        // in future it may be useful to be able to insert file names with drag-and-drop
        // without quoting them (this only affects paths with spaces in)
        dropText += KShell::quoteArg(url.isLocalFile() ? url.path() : url.url());

        // Each filename(including the last) should be followed by one space.
        dropText += QLatin1Char(' ');
    }
    return dropText;
}

void setupCdToUrlAction(const QString& dropText, const QUrl& url, QList<QAction*>& additionalActions, TerminalDisplay *display) {
    KIO::StatJob* job = KIO::mostLocalUrl(url, KIO::HideProgressInfo);
    if (!job->exec()) {
        return;
    }

    const QUrl localUrl = job->mostLocalUrl();
    if (!localUrl.isLocalFile()) {
        return;
    }

    const QFileInfo fileInfo(localUrl.path());
    if (!fileInfo.isDir()) {
        return;
    }

    QAction* cdAction = new QAction(i18n("Change &Directory To"), display);
    const QByteArray triggerText = QString(QLatin1String(" cd ") + dropText + QLatin1Char('\n')).toLocal8Bit();
    display->connect(cdAction, &QAction::triggered, display,  [display, triggerText]{
        emit display->sendStringToEmu(triggerText);} );
    additionalActions.append(cdAction);
}

}

void TerminalDisplay::dropEvent(QDropEvent* event)
{
    if (_readOnly) {
        event->accept();
        return;
    }

    const auto mimeData = event->mimeData();
    if (mimeData == nullptr) {
        return;
    }
    auto urls = mimeData->urls();

    QString dropText;
    if (!urls.isEmpty()) {
        dropText = extractDroppedText(urls);

        // If our target is local we will open a popup - otherwise the fallback kicks
        // in and the URLs will simply be pasted as text.
        if (!_dropUrlsAsText && (_sessionController != nullptr) && _sessionController->url().isLocalFile()) {
            // A standard popup with Copy, Move and Link as options -
            // plus an additional Paste option.

            QAction* pasteAction = new QAction(i18n("&Paste Location"), this);
            connect(pasteAction, &QAction::triggered, this, [this, dropText]{ emit sendStringToEmu(dropText.toLocal8Bit());} );

            QList<QAction*> additionalActions;
            additionalActions.append(pasteAction);

            if (urls.count() == 1) {
                setupCdToUrlAction(dropText, urls.at(0), additionalActions, this);
            }

            QUrl target = QUrl::fromLocalFile(_sessionController->currentDir());

            KIO::DropJob* job = KIO::drop(event, target);
            KJobWidgets::setWindow(job, this);
            job->setApplicationActions(additionalActions);
            return;
        }

    } else {
        dropText = mimeData->text();
    }

    if (mimeData->hasFormat(QStringLiteral("text/plain")) ||
            mimeData->hasFormat(QStringLiteral("text/uri-list"))) {
        emit sendStringToEmu(dropText.toLocal8Bit());
    }
}

void TerminalDisplay::doDrag()
{
    const QMimeData *clipboardMimeData = QApplication::clipboard()->mimeData(QClipboard::Selection);
    if (clipboardMimeData == nullptr) {
        return;
    }
    auto mimeData = new QMimeData();
    _dragInfo.state = diDragging;
    _dragInfo.dragObject = new QDrag(this);
    mimeData->setText(clipboardMimeData->text());
    mimeData->setHtml(clipboardMimeData->html());
    _dragInfo.dragObject->setMimeData(mimeData);
    _dragInfo.dragObject->exec(Qt::CopyAction);
}

void TerminalDisplay::setSessionController(SessionController* controller)
{
    _sessionController = controller;
    _headerBar->finishHeaderSetup(controller);
}

SessionController* TerminalDisplay::sessionController()
{
    return _sessionController;
}

IncrementalSearchBar *TerminalDisplay::searchBar() const
{
    return _searchBar;
}

AutoScrollHandler::AutoScrollHandler(QWidget* parent)
    : QObject(parent)
    , _timerId(0)
{
    parent->installEventFilter(this);
}
void AutoScrollHandler::timerEvent(QTimerEvent* event)
{
    if (event->timerId() != _timerId) {
        return;
    }

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
    Q_UNUSED(watched)

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        bool mouseInWidget = widget()->rect().contains(mouseEvent->pos());
        if (mouseInWidget) {
            if (_timerId != 0) {
                killTimer(_timerId);
            }

            _timerId = 0;
        } else {
            if ((_timerId == 0) && ((mouseEvent->buttons() & Qt::LeftButton) != 0u)) {
                _timerId = startTimer(100);
            }
        }

        break;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if ((_timerId != 0) && ((mouseEvent->buttons() & ~Qt::LeftButton) != 0u)) {
            killTimer(_timerId);
            _timerId = 0;
        }
        break;
    }
    default:
        break;
    }

    return false;
}

void TerminalDisplay::applyProfile(const Profile::Ptr &profile)
{
    // load color scheme
    ColorEntry table[TABLE_COLORS];
    _colorScheme = ViewManager::colorSchemeForProfile(profile);
    _colorScheme->getColorTable(table, randomSeed());
    setColorTable(table);
    setOpacity(_colorScheme->opacity());
    setWallpaper(_colorScheme->wallpaper());

    // load font
    _antialiasText = profile->antiAliasFonts();
    _boldIntense = profile->boldIntense();
    _useFontLineCharacters = profile->useFontLineCharacters();
    setVTFont(profile->font());

    // set scroll-bar position
    setScrollBarPosition(Enum::ScrollBarPositionEnum(profile->property<int>(Profile::ScrollBarPosition)));
    setScrollFullPage(profile->property<bool>(Profile::ScrollFullPage));

    // show hint about terminal size after resizing
    _showTerminalSizeHint = profile->showTerminalSizeHint();
    _dimWhenInactive = profile->dimWhenInactive();

    // terminal features
    setBlinkingCursorEnabled(profile->blinkingCursorEnabled());
    setBlinkingTextEnabled(profile->blinkingTextEnabled());
    _tripleClickMode = Enum::TripleClickModeEnum(profile->property<int>(Profile::TripleClickMode));
    setAutoCopySelectedText(profile->autoCopySelectedText());
    _ctrlRequiredForDrag = profile->property<bool>(Profile::CtrlRequiredForDrag);
    _dropUrlsAsText = profile->property<bool>(Profile::DropUrlsAsText);
    _bidiEnabled = profile->bidiRenderingEnabled();
    setLineSpacing(uint(profile->lineSpacing()));
    _trimLeadingSpaces = profile->property<bool>(Profile::TrimLeadingSpacesInSelectedText);
    _trimTrailingSpaces = profile->property<bool>(Profile::TrimTrailingSpacesInSelectedText);
    _openLinksByDirectClick = profile->property<bool>(Profile::OpenLinksByDirectClickEnabled);
    _urlHintsModifiers = Qt::KeyboardModifiers(profile->property<int>(Profile::UrlHintsModifiers));
    _reverseUrlHints = profile->property<bool>(Profile::ReverseUrlHints);
    setMiddleClickPasteMode(Enum::MiddleClickPasteModeEnum(profile->property<int>(Profile::MiddleClickPasteMode)));
    setCopyTextAsHTML(profile->property<bool>(Profile::CopyTextAsHTML));

    // highlight lines scrolled into view (must be applied before margin/center)
    setHighlightScrolledLines(profile->property<bool>(Profile::HighlightScrolledLines));
    _highlightScrolledLinesControl.needToClear = true;

    // margin/center
    setMargin(profile->property<int>(Profile::TerminalMargin));
    setCenterContents(profile->property<bool>(Profile::TerminalCenter));

    // cursor shape
    setKeyboardCursorShape(Enum::CursorShapeEnum(profile->property<int>(Profile::CursorShape)));

    // cursor color
    // an invalid QColor is used to inform the view widget to
    // draw the cursor using the default color( matching the text)
    setKeyboardCursorColor(profile->useCustomCursorColor() ? profile->customCursorColor() : QColor());
    setKeyboardCursorTextColor(profile->useCustomCursorColor() ? profile->customCursorTextColor() : QColor());

    // word characters
    setWordCharacters(profile->wordCharacters());

    // bell mode
    setBellMode(profile->property<int>(Profile::BellMode));

    // mouse wheel zoom
    _mouseWheelZoom = profile->mouseWheelZoomEnabled();
    setAlternateScrolling(profile->property<bool>(Profile::AlternateScrolling));
}

//
// CompositeWidgetFocusWatcher
//

CompositeWidgetFocusWatcher::CompositeWidgetFocusWatcher(QWidget *compositeWidget, QObject *parent)
    : QObject(parent)
    , _compositeWidget(compositeWidget)
{
    registerWidgetAndChildren(compositeWidget);
}

bool CompositeWidgetFocusWatcher::eventFilter(QObject *watched, QEvent *event)
{
    Q_UNUSED(watched)

    auto *focusEvent = static_cast<QFocusEvent *>(event);
    switch(event->type()) {
    case QEvent::FocusIn:
        emit compositeFocusChanged(true);
        break;
    case QEvent::FocusOut:
        if(focusEvent->reason() != Qt::PopupFocusReason) {
            emit compositeFocusChanged(false);
        }
        break;
    default:
        break;
    }
    return false;
}

void CompositeWidgetFocusWatcher::registerWidgetAndChildren(QWidget *widget)
{
    Q_ASSERT(widget != nullptr);

    if (widget->focusPolicy() != Qt::NoFocus) {
        widget->installEventFilter(this);
    }
    for (auto *child: widget->children()) {
        auto *childWidget = qobject_cast<QWidget *>(child);
        if (childWidget != nullptr) {
            registerWidgetAndChildren(childWidget);
        }
    }
}
