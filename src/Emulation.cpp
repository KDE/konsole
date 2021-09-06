/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>
    SPDX-FileCopyrightText: 1996 Matthias Ettrich <ettrich@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "Emulation.h"

// Qt
#include <QKeyEvent>

// Konsole
#include "Screen.h"
#include "ScreenWindow.h"
#include "keyboardtranslator/KeyboardTranslator.h"
#include "keyboardtranslator/KeyboardTranslatorManager.h"

using namespace Konsole;

Emulation::Emulation()
    : _windows(QList<ScreenWindow *>())
    , _currentScreen(nullptr)
    , _codec(nullptr)
    , _keyTranslator(nullptr)
    , _usesMouseTracking(false)
    , _bracketedPasteMode(false)
    , _bulkTimer1(QTimer(this))
    , _bulkTimer2(QTimer(this))
    , _imageSizeInitialized(false)
    , _peekingPrimary(false)
    , _activeScreenIndex(0)
{
    // create screens with a default size
    _screen[0] = new Screen(40, 80);
    _screen[1] = new Screen(40, 80);
    _currentScreen = _screen[0];

    QObject::connect(&_bulkTimer1, &QTimer::timeout, this, &Konsole::Emulation::showBulk);
    QObject::connect(&_bulkTimer2, &QTimer::timeout, this, &Konsole::Emulation::showBulk);

    // listen for mouse status changes
    connect(this, &Konsole::Emulation::programRequestsMouseTracking, this, &Konsole::Emulation::setUsesMouseTracking);
    connect(this, &Konsole::Emulation::programBracketedPasteModeChanged, this, &Konsole::Emulation::bracketedPasteModeChanged);
}

bool Emulation::programUsesMouseTracking() const
{
    return _usesMouseTracking;
}

void Emulation::setUsesMouseTracking(bool usesMouseTracking)
{
    _usesMouseTracking = usesMouseTracking;
}

bool Emulation::programBracketedPasteMode() const
{
    return _bracketedPasteMode;
}

void Emulation::bracketedPasteModeChanged(bool bracketedPasteMode)
{
    _bracketedPasteMode = bracketedPasteMode;
}

ScreenWindow *Emulation::createWindow()
{
    auto window = new ScreenWindow(_currentScreen);
    _windows << window;

    connect(window, &Konsole::ScreenWindow::selectionChanged, this, &Konsole::Emulation::bufferedUpdate);
    connect(window, &Konsole::ScreenWindow::selectionChanged, this, &Konsole::Emulation::checkSelectedText);

    connect(this, &Konsole::Emulation::outputChanged, window, &Konsole::ScreenWindow::notifyOutputChanged);

    return window;
}

void Emulation::setCurrentTerminalDisplay(TerminalDisplay *display)
{
    _screen[0]->setCurrentTerminalDisplay(display);
    _screen[1]->setCurrentTerminalDisplay(display);
}

void Emulation::checkScreenInUse()
{
    Q_EMIT primaryScreenInUse(_currentScreen == _screen[0]);
}

void Emulation::checkSelectedText()
{
    QString text = _currentScreen->selectedText(Screen::PreserveLineBreaks);
    Q_EMIT selectionChanged(text);
}

Emulation::~Emulation()
{
    for (ScreenWindow *window : qAsConst(_windows)) {
        delete window;
    }

    delete _screen[0];
    delete _screen[1];
}

void Emulation::setPeekPrimary(const bool doPeek)
{
    if (doPeek == _peekingPrimary) {
        return;
    }
    _peekingPrimary = doPeek;
    setScreenInternal(doPeek ? 0 : _activeScreenIndex);
    Q_EMIT outputChanged();
}

void Emulation::setScreen(int index)
{
    _activeScreenIndex = index;
    _peekingPrimary = false;
    setScreenInternal(_activeScreenIndex);
}

void Emulation::setScreenInternal(int index)
{
    Screen *oldScreen = _currentScreen;
    _currentScreen = _screen[index & 1];
    if (_currentScreen != oldScreen) {
        // tell all windows onto this emulation to switch to the newly active screen
        for (ScreenWindow *window : qAsConst(_windows)) {
            window->setScreen(_currentScreen);
        }

        checkScreenInUse();
        checkSelectedText();
    }
}

void Emulation::clearHistory()
{
    _screen[0]->setScroll(_screen[0]->getScroll(), false);
}

void Emulation::setHistory(const HistoryType &history)
{
    _screen[0]->setScroll(history);

    showBulk();
}

const HistoryType &Emulation::history() const
{
    return _screen[0]->getScroll();
}

void Emulation::setCodec(const QTextCodec *codec)
{
    if (codec != nullptr) {
        _codec = codec;

        _decoder.reset(_codec->makeDecoder());

        Q_EMIT useUtf8Request(utf8());
    } else {
        setCodec(LocaleCodec);
    }
}

void Emulation::setCodec(EmulationCodec codec)
{
    if (codec == Utf8Codec) {
        setCodec(QTextCodec::codecForName("utf8"));
    } else if (codec == LocaleCodec) {
        setCodec(QTextCodec::codecForLocale());
    }
}

void Emulation::setKeyBindings(const QString &name)
{
    _keyTranslator = KeyboardTranslatorManager::instance()->findTranslator(name);
    if (_keyTranslator == nullptr) {
        _keyTranslator = KeyboardTranslatorManager::instance()->defaultTranslator();
    }
}

QString Emulation::keyBindings() const
{
    return _keyTranslator->name();
}

// process application unicode input to terminal
// this is a trivial scanner
void Emulation::receiveChar(uint c)
{
    c &= 0xff;
    switch (c) {
    case '\b':
        _currentScreen->backspace();
        break;
    case '\t':
        _currentScreen->tab();
        break;
    case '\n':
        _currentScreen->newLine();
        break;
    case '\r':
        _currentScreen->toStartOfLine();
        break;
    case 0x07:
        Q_EMIT bell();
        break;
    default:
        _currentScreen->displayCharacter(c);
        break;
    }
}

void Emulation::sendKeyEvent(QKeyEvent *ev)
{
    if (!ev->text().isEmpty()) {
        // A block of text
        // Note that the text is proper unicode.
        // We should do a conversion here
        Q_EMIT sendData(ev->text().toLocal8Bit());
    }
}

void Emulation::receiveData(const char *text, int length)
{
    Q_ASSERT(_decoder);

    bufferedUpdate();

    // send characters to terminal emulator
    for (const uint i : _decoder->toUnicode(text, length).toUcs4()) {
        receiveChar(i);
    }

    // look for z-modem indicator
    //-- someone who understands more about z-modems that I do may be able to move
    // this check into the above for loop?
    for (int i = 0; i < length - 4; i++) {
        if (text[i] == '\030') {
            if (qstrncmp(text + i + 1, "B00", 3) == 0) {
                Q_EMIT zmodemDownloadDetected();
            } else if (qstrncmp(text + i + 1, "B01", 3) == 0) {
                Q_EMIT zmodemUploadDetected();
            }
        }
    }
}

void Emulation::writeToStream(TerminalCharacterDecoder *decoder, int startLine, int endLine)
{
    _currentScreen->writeLinesToStream(decoder, startLine, endLine);
}

int Emulation::lineCount() const
{
    // sum number of lines currently on _screen plus number of lines in history
    return _currentScreen->getLines() + _currentScreen->getHistLines();
}

void Emulation::showBulk()
{
    _bulkTimer1.stop();
    _bulkTimer2.stop();

    Q_EMIT outputChanged();

    _currentScreen->resetScrolledLines();
    _currentScreen->resetDroppedLines();
}

void Emulation::bufferedUpdate()
{
    static const int BULK_TIMEOUT1 = 10;
    static const int BULK_TIMEOUT2 = 40;

    _bulkTimer1.setSingleShot(true);
    _bulkTimer1.start(BULK_TIMEOUT1);
    if (!_bulkTimer2.isActive()) {
        _bulkTimer2.setSingleShot(true);
        _bulkTimer2.start(BULK_TIMEOUT2);
    }
}

char Emulation::eraseChar() const
{
    return '\b';
}

void Emulation::setImageSize(int lines, int columns)
{
    if ((lines < 1) || (columns < 1)) {
        return;
    }

    QSize screenSize[2] = {QSize(_screen[0]->getColumns(), _screen[0]->getLines()), //
                           QSize(_screen[1]->getColumns(), _screen[1]->getLines())};
    QSize newSize(columns, lines);

    if (newSize == screenSize[0] && newSize == screenSize[1]) {
        // If this method is called for the first time, always emit
        // SIGNAL(imageSizeChange()), even if the new size is the same as the
        // current size.  See #176902
        if (!_imageSizeInitialized) {
            Q_EMIT imageSizeChanged(lines, columns);
        }
    } else {
        _screen[0]->resizeImage(lines, columns);
        _screen[1]->resizeImage(lines, columns);

        Q_EMIT imageSizeChanged(lines, columns);

        bufferedUpdate();
    }

    if (!_imageSizeInitialized) {
        _imageSizeInitialized = true;

        Q_EMIT imageSizeInitialized();
    }
}

QSize Emulation::imageSize() const
{
    return {_currentScreen->getColumns(), _currentScreen->getLines()};
}
