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
#include "terminalDisplay/TerminalDisplay.h"
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
#include "extras/CompositeWidgetFocusWatcher.h"
#include "extras/AutoScrollHandler.h"

#include "filterHotSpots/Filter.h"
#include "filterHotSpots/TerminalImageFilterChain.h"
#include "filterHotSpots/HotSpot.h"
#include "filterHotSpots/FileFilterHotspot.h"
#include "filterHotSpots/EscapeSequenceUrlFilter.h"
#include "filterHotSpots/EscapeSequenceUrlFilterHotSpot.h"

#include "konsoledebug.h"
#include "PlainTextDecoder.h"
#include "Screen.h"
#include "ExtendedCharTable.h"
#include "../widgets/TerminalDisplayAccessible.h"
#include "session/SessionController.h"
#include "session/SessionManager.h"
#include "session/Session.h"
#include "WindowSystemInfo.h"
#include "widgets/IncrementalSearchBar.h"
#include "profile/Profile.h"
#include "ViewManager.h" // for colorSchemeForProfile. // TODO: Rewrite this.
#include "../characters/LineBlockCharacters.h"
#include "PrintOptions.h"
#include "../widgets/KonsolePrintManager.h"
#include "EscapeSequenceUrlExtractor.h"

#include "TerminalPainter.hpp"
#include "TerminalScrollBar.hpp"

using namespace Konsole;

#define REPCHAR   "ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    "abcdefgjijklmnopqrstuvwxyz" \
    "0123456789./+@"

inline int TerminalDisplay::loc(int x, int y) const {
    if (y < 0 || y > _lines) {
        qDebug() << "Y: " << y << "Lines" << _lines;
    }
    if (x < 0 || x > _columns ) {
        qDebug() << "X" << x << "Columns" << _columns;
    }

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
            _iPntSel = QPoint(-1, -1);
            _pntSel = QPoint(-1, -1);
            _tripleSelBegin = QPoint(-1, -1);
        });
        connect(_screenWindow.data(), &Konsole::ScreenWindow::scrolled, this, [this]() {
            _filterUpdateRequired = true;
        });
        connect(_screenWindow.data(), &Konsole::ScreenWindow::outputChanged, this, []() {
            QGuiApplication::inputMethod()->update(Qt::ImCursorRectangle);
        });
        _screenWindow->setWindowLines(_lines);

        auto profile = SessionManager::instance()->sessionProfile(_sessionController->session());
        _screenWindow->screen()->urlExtractor()->setAllowedLinkSchema(profile->escapedLinksSchema());
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
    , _bracketedPasteMode(false)
    , _iPntSel(QPoint(-1, -1))
    , _pntSel(QPoint(-1, -1))
    , _tripleSelBegin(QPoint(-1, -1))
    , _actSel(0)
    , _wordSelectionMode(false)
    , _lineSelectionMode(false)
    , _preserveLineBreaks(true)
    , _columnSelectionMode(false)
    , _autoCopySelectedText(false)
    , _copyTextAsHTML(true)
    , _middleClickPasteMode(Enum::PasteFromX11Selection)
    , _wordCharacters(QStringLiteral(":@-./_~"))
    , _bellMode(Enum::NotifyBell)
    , _allowBlinkingText(true)
    , _allowBlinkingCursor(false)
    , _textBlinking(false)
    , _cursorBlinking(false)
    , _hasTextBlinker(false)
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
    , _filterChain(new TerminalImageFilterChain(this))
    , _filterUpdateRequired(true)
    , _cursorShape(Enum::BlockCursor)
    , _cursorColor(QColor())
    , _cursorTextColor(QColor())
    , _antialiasText(true)
    , _useFontLineCharacters(false)
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
    , _scrollBar(nullptr)
    , _printManager(nullptr)
{
    // terminal applications are not designed with Right-To-Left in mind,
    // so the layout is forced to Left-To-Right
    setLayoutDirection(Qt::LeftToRight);

    _contentRect = QRect(_margin, _margin, 1, 1);

    // create scroll bar for scrolling output up and down
    _scrollBar = new TerminalScrollBar(this);
    _scrollBar->setAutoFillBackground(false);
    // set the scroll bar's slider to occupy the whole area of the scroll bar initially
    _scrollBar->setScroll(0, 0);
    _scrollBar->setCursor(Qt::ArrowCursor);
    _headerBar->setCursor(Qt::ArrowCursor);
    connect(_headerBar, &TerminalHeaderBar::requestToggleExpansion, this, &Konsole::TerminalDisplay::requestToggleExpansion);
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
    CompositeWidgetFocusWatcher *focusWatcher = new CompositeWidgetFocusWatcher(this);
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

    _terminalPainter = new TerminalPainter(this);
    
    auto ldrawBackground = [this](QPainter &painter,
            const QRect &rect, const QColor &backgroundColor, bool useOpacitySetting) {
        _terminalPainter->drawBackground(painter, rect, backgroundColor, useOpacitySetting);
    };
    auto ldrawContents = [this](QPainter &paint, const QRect &rect, bool friendly) {
        _terminalPainter->drawContents(paint, rect, friendly);
    };
    auto lgetBackgroundColor = [this]() {
        return getBackgroundColor();
    };
    _printManager = new KonsolePrintManager(ldrawBackground, ldrawContents, lgetBackgroundColor);
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

    delete _terminalPainter;
    delete _printManager;
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

void TerminalDisplay::scrollScreenWindow(enum ScreenWindow::RelativeScrollMode mode, int amount) 
{
    _screenWindow->scrollBy(mode, amount, _scrollBar->scrollFullPage());
    _screenWindow->setTrackOutput(_screenWindow->atEndOfOutput());
    updateLineProperties();
    updateImage();
    viewScrolledByUser();
}

void TerminalDisplay::setRandomSeed(uint randomSeed)
{
    _randomSeed = randomSeed;
}
uint TerminalDisplay::randomSeed() const
{
    return _randomSeed;
}

void TerminalDisplay::processFilters()
{

    if (_screenWindow.isNull()) {
        return;
    }

    if (!_filterUpdateRequired) {
        return;
    }

    const QRegion preUpdateHotSpots = _filterChain->hotSpotRegion();

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

    const QRegion postUpdateHotSpots = _filterChain->hotSpotRegion();

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
        _scrollBar->scrollImage(_screenWindow->scrollCount() ,
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

    _scrollBar->setScroll(_screenWindow->currentLine() , _screenWindow->lineCount());

    Q_ASSERT(_usedLines <= _lines);
    Q_ASSERT(_usedColumns <= _columns);

    int y;
    int x;
    int len;

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
        dirtyRegion |= _terminalPainter->highlightScrolledLinesRegion(dirtyRegion.isEmpty());
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
        _terminalPainter->drawBackground(paint, rect, getBackgroundColor(), true /* use opacity setting */);
    }

    if (_displayVerticalLine) {
        const int x = (_fontWidth/2) + (_fontWidth * _displayVerticalLineAtChar);
        const QColor lineColor = getForegroundColor();

        paint.setPen(lineColor);
        paint.drawLine(QPoint(x, 0), QPoint(x, height()));
    }

    // only turn on text anti-aliasing, never turn on normal antialiasing
    // set https://bugreports.qt.io/browse/QTBUG-66036
    paint.setRenderHint(QPainter::TextAntialiasing, _antialiasText);

    for (const QRect &rect : qAsConst(dirtyImageRegion)) {
        _terminalPainter->drawContents(paint, rect, false);
    }
    _terminalPainter->drawCurrentResultRect(paint);
    _terminalPainter->highlightScrolledLines(paint);
    _terminalPainter->drawInputMethodPreeditString(paint, preeditRect());
    paintFilters(paint);

    const bool drawDimmed = _dimWhenInactive && !hasFocus();
    if (drawDimmed) {
        const QColor dimColor(0, 0, 0, _dimValue);
        for (const QRect &rect : region) {
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

QPoint TerminalDisplay::cursorPosition() const
{
    if (!_screenWindow.isNull()) {
        return _screenWindow->cursorPosition();
    } else {
        return {0, 0};
    }
}

bool TerminalDisplay::isCursorOnDisplay() const
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


    _filterChain->paint(this, painter);
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

    const auto scrollBarWidth = _scrollBar->scrollBarPosition() != Enum::ScrollBarHidden
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

    switch (_scrollBar->scrollBarPosition()) {
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

    processFilters();

    _filterChain->mouseMoveEvent(this, ev, charLine, charColumn);

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
    } else if (ev->button() == Qt::MiddleButton) {
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

QSharedPointer<HotSpot> TerminalDisplay::filterActions(const QPoint& position)
{
    int charLine;
    int charColumn;
    getCharacterPosition(position, charLine, charColumn, false);

    return _filterChain->hotSpotAt(charLine, charColumn);
}

void TerminalDisplay::mouseMoveEvent(QMouseEvent* ev)
{
    int charLine = 0;
    int charColumn = 0;

    if (!hasFocus() && KonsoleSettings::focusFollowsMouse()) {
       setFocus();
    }

    getCharacterPosition(ev->pos(), charLine, charColumn, !_usesMouseTracking);

    processFilters();

    _filterChain->mouseMoveEvent(this, ev, charLine, charColumn);
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
        if ((ev->buttons() & Qt::MiddleButton) != 0u) {
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
    if ((ev->buttons() & Qt::MiddleButton) != 0u) {
        return;
    }

    extendSelection(ev->pos());
}

void TerminalDisplay::leaveEvent(QEvent *ev)
{
    // remove underline from an active link when cursor leaves the widget area,
    // also restore regular mouse cursor shape
    _filterChain->leaveEvent(this, ev);
}

void TerminalDisplay::extendSelection(const QPoint& position)
{
    if (_screenWindow.isNull()) {
        return;
    }

    if (_iPntSel.x() < 0 || _iPntSel.y() < 0 || _pntSel.x() < 0 || _pntSel.y() < 0) {
        _iPntSel = _pntSel = position;
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
    QPoint iPntSelCorr = _iPntSel;
    iPntSelCorr.ry() -= _scrollBar->value();
    QPoint pntSelCorr = _pntSel;
    pntSelCorr.ry() -= _scrollBar->value();
    bool swapping = false;

    if (_wordSelectionMode) {
        // Extend to word boundaries
        const bool left_not_right = (here.y() < iPntSelCorr.y() ||
                                     (here.y() == iPntSelCorr.y() && here.x() < iPntSelCorr.x()));
        const bool old_left_not_right = (pntSelCorr.y() < iPntSelCorr.y() ||
                                         (pntSelCorr.y() == iPntSelCorr.y() && pntSelCorr.x() < iPntSelCorr.x()));
        swapping = left_not_right != old_left_not_right;

        // Find left (left_not_right ? from here : from start of word)
        QPoint left = left_not_right ? here : iPntSelCorr;
        // Find left (left_not_right ? from end of word : from here)
        QPoint right = left_not_right ? iPntSelCorr : here;

        if (left.y() < 0 || left.y() >= _lines || left.x() < 0 || left.x() >= _columns) {
            left = pntSelCorr;
        } else {
            left = findWordStart(left);
        }
        if (right.y() < 0 || right.y() >= _lines || right.x() < 0 || right.x() >= _columns) {
            right = pntSelCorr;
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
        const bool above_not_below = (here.y() < iPntSelCorr.y());
        if (above_not_below) {
            ohere = findLineEnd(iPntSelCorr);
            here = findLineStart(here);
        } else {
            ohere = findLineStart(iPntSelCorr);
            here = findLineEnd(here);
        }

        swapping = !(_tripleSelBegin == ohere);
        _tripleSelBegin = ohere;

        ohere.rx()++;
    }

    int offset = 0;
    if (!_wordSelectionMode && !_lineSelectionMode) {
        const bool left_not_right = (here.y() < iPntSelCorr.y() ||
                                     (here.y() == iPntSelCorr.y() && here.x() < iPntSelCorr.x()));
        const bool old_left_not_right = (pntSelCorr.y() < iPntSelCorr.y() ||
                                         (pntSelCorr.y() == iPntSelCorr.y() && pntSelCorr.x() < iPntSelCorr.x()));
        swapping = left_not_right != old_left_not_right;

        // Find left (left_not_right ? from here : from start)
        const QPoint left = left_not_right ? here : iPntSelCorr;

        // Find right (left_not_right ? from start : from here)
        QPoint right = left_not_right ? iPntSelCorr : here;

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

    if ((here == pntSelCorr) && (scroll == _scrollBar->value())) {
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
            (ev->button() == Qt::RightButton || ev->button() == Qt::MiddleButton) &&
            !(ev->modifiers() & Qt::ShiftModifier)) {
        emit mouseSignal(ev->button() == Qt::MiddleButton ? 1 : 2,
                         charColumn + 1,
                         charLine + 1 + _scrollBar->value() - _scrollBar->maximum() ,
                         2);
    }

    if (!_screenWindow->screen()->hasSelection()) {
        _filterChain->mouseReleaseEvent(this, ev, charLine, charColumn);
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
    if (ev->button() == Qt::MiddleButton) {
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
        _scrollBar->applyScrollBarPosition(false);

        Q_ASSERT(_sessionController != nullptr);

        _sessionController->setSearchStartToWindowCurrentLine();
        _scrollWheelState.clearAll();
    } else if (!_readOnly) {
        _scrollWheelState.addWheelEvent(ev);

        Q_ASSERT(!_sessionController->session().isNull());

        if(!_usesMouseTracking && !_sessionController->session()->isPrimaryScreen() && _scrollBar->alternateScrolling()) {
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
    resetCursor();
}

void TerminalDisplay::resetCursor()
{
    setCursor(_usesMouseTracking ? Qt::ArrowCursor : Qt::IBeamCursor);
}

bool TerminalDisplay::usesMouseTracking() const
{
    return _usesMouseTracking;
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

void TerminalDisplay::keyPressEvent(QKeyEvent* event)
{
    { // C++17: change getCharacterPosition to return a tuple and use auto [charLine, charColumn] to extract the values.
        int charLine;
        int charColumn;
        getCharacterPosition(mapFromGlobal(QCursor::pos()), charLine, charColumn, !_usesMouseTracking);

        // Don't process it if the filterchain handled it for us
        if (_filterChain->keyPressEvent(this, event, charLine, charColumn)) {
            return;
        }
    }

    if (!_peekPrimaryShortcut.isEmpty() && _peekPrimaryShortcut.matches(QKeySequence(event->key() | event->modifiers()))) {
        peekPrimaryRequested(true);
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
    if (_readOnly) {
        event->accept();
        return;
    }

    { // C++17: change getCharacterPosition to return a tuple and use auto [charLine, charColumn] to extract the values.
        int charLine;
        int charColumn;
        getCharacterPosition(mapFromGlobal(QCursor::pos()), charLine, charColumn, !_usesMouseTracking);
        _filterChain->keyReleaseEvent(this, event, charLine, charColumn);
    }

    peekPrimaryRequested(false);

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

    setFocus(Qt::MouseFocusReason);
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
    _scrollBar->setScrollBarPosition(Enum::ScrollBarPositionEnum(profile->property<int>(Profile::ScrollBarPosition)));
    _scrollBar->setScrollFullPage(profile->property<bool>(Profile::ScrollFullPage));

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
    setMiddleClickPasteMode(Enum::MiddleClickPasteModeEnum(profile->property<int>(Profile::MiddleClickPasteMode)));
    setCopyTextAsHTML(profile->property<bool>(Profile::CopyTextAsHTML));

    // highlight lines scrolled into view (must be applied before margin/center)
    _scrollBar->setHighlightScrolledLines(profile->property<bool>(Profile::HighlightScrolledLines));
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

    _displayVerticalLine = profile->verticalLine();
    _displayVerticalLineAtChar = profile->verticalLineAtChar();
    _scrollBar->setAlternateScrolling(profile->property<bool>(Profile::AlternateScrolling));
    _dimValue = profile->dimValue();

    _filterChain->setUrlHintsModifiers(Qt::KeyboardModifiers(profile->property<int>(Profile::UrlHintsModifiers)));
    _filterChain->setReverseUrlHints(profile->property<bool>(Profile::ReverseUrlHints));

    _peekPrimaryShortcut = profile->peekPrimaryKeySequence();
}

void TerminalDisplay::printScreen()
{
    auto lprintContent = [this](QPainter &painter, bool friendly) {
        
        QPoint columnLines(_usedLines, _usedColumns);
        auto lfontget = [this]() { return getVTFont(); };
        auto lfontset = [this](const QFont &f) { setVTFont(f); };

        _printManager->printContent(painter, friendly, columnLines, lfontget, lfontset);
    };
    _printManager->printRequest(lprintContent, this);
}

Character TerminalDisplay::getCursorCharacter(int column, int line)
{
    return _image[loc(column, line)];
}
