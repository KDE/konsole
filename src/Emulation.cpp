/*
    Copyright 2007-2008 Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>
    Copyright 1996 by Matthias Ettrich <ettrich@kde.org>

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
#include "Emulation.h"

// Qt
#include <QKeyEvent>

// Konsole
#include "KeyboardTranslator.h"
#include "KeyboardTranslatorManager.h"
#include "Screen.h"
#include "ScreenWindow.h"

using namespace Konsole;

Emulation::Emulation() :
    _windows(QList<ScreenWindow *>()),
    _currentScreen(nullptr),
    _codec(nullptr),
    _decoder(nullptr),
    _keyTranslator(nullptr),
    _usesMouseTracking(false),
    _bracketedPasteMode(false),
    _bulkTimer1(new QTimer(this)),
    _bulkTimer2(new QTimer(this)),
    _imageSizeInitialized(false)
{
    // create screens with a default size
    _screen[0] = new Screen(40, 80);
    _screen[1] = new Screen(40, 80);
    _currentScreen = _screen[0];

    QObject::connect(&_bulkTimer1, &QTimer::timeout, this, &Konsole::Emulation::showBulk);
    QObject::connect(&_bulkTimer2, &QTimer::timeout, this, &Konsole::Emulation::showBulk);

    // listen for mouse status changes
    connect(this, &Konsole::Emulation::programRequestsMouseTracking, this,
            &Konsole::Emulation::setUsesMouseTracking);
    connect(this, &Konsole::Emulation::programBracketedPasteModeChanged, this,
            &Konsole::Emulation::bracketedPasteModeChanged);
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

    connect(window, &Konsole::ScreenWindow::selectionChanged, this,
            &Konsole::Emulation::bufferedUpdate);
    connect(window, &Konsole::ScreenWindow::selectionChanged, this,
            &Konsole::Emulation::checkSelectedText);

    connect(this, &Konsole::Emulation::outputChanged, window,
            &Konsole::ScreenWindow::notifyOutputChanged);

    return window;
}

void Emulation::checkScreenInUse()
{
    emit primaryScreenInUse(_currentScreen == _screen[0]);
}

void Emulation::checkSelectedText()
{
    QString text = _currentScreen->selectedText(Screen::PreserveLineBreaks);
    emit selectionChanged(text);
}

Emulation::~Emulation()
{
    foreach (ScreenWindow *window, _windows) {
        delete window;
    }

    delete _screen[0];
    delete _screen[1];
    delete _decoder;
}

void Emulation::setScreen(int index)
{
    Screen *oldScreen = _currentScreen;
    _currentScreen = _screen[index & 1];
    if (_currentScreen != oldScreen) {
        // tell all windows onto this emulation to switch to the newly active screen
        foreach (ScreenWindow *window, _windows) {
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

        delete _decoder;
        _decoder = _codec->makeDecoder();

        emit useUtf8Request(utf8());
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
        emit stateSet(NOTIFYBELL);
        break;
    default:
        _currentScreen->displayCharacter(c);
        break;
    }
}

void Emulation::sendKeyEvent(QKeyEvent *ev)
{
    emit stateSet(NOTIFYNORMAL);

    if (!ev->text().isEmpty()) {
        // A block of text
        // Note that the text is proper unicode.
        // We should do a conversion here
        emit sendData(ev->text().toLocal8Bit());
    }
}

void Emulation::sendMouseEvent(int /*buttons*/, int /*column*/, int /*row*/, int /*eventType*/)
{
    // default implementation does nothing
}

/*
   We are doing code conversion from locale to unicode first.
*/

void Emulation::receiveData(const char *text, int length)
{
    emit stateSet(NOTIFYACTIVITY);

    bufferedUpdate();

    QVector<uint> unicodeText = _decoder->toUnicode(text, length).toUcs4();

    //send characters to terminal emulator
    for (auto &&i : unicodeText) {
        receiveChar(i);
    }

    //look for z-modem indicator
    //-- someone who understands more about z-modems that I do may be able to move
    //this check into the above for loop?
    for (int i = 0; i < length; i++) {
        if (text[i] == '\030') {
            if (length - i - 1 > 3) {
                if (qstrncmp(text + i + 1, "B00", 3) == 0) {
                    emit zmodemDownloadDetected();
                } else if (qstrncmp(text + i + 1, "B01", 3) == 0) {
                    emit zmodemUploadDetected();
                }
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

    emit outputChanged();

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

    QSize screenSize[2] = {
        QSize(_screen[0]->getColumns(),
              _screen[0]->getLines()),
        QSize(_screen[1]->getColumns(),
              _screen[1]->getLines())
    };
    QSize newSize(columns, lines);

    if (newSize == screenSize[0] && newSize == screenSize[1]) {
        // If this method is called for the first time, always emit
        // SIGNAL(imageSizeChange()), even if the new size is the same as the
        // current size.  See #176902
        if (!_imageSizeInitialized) {
            emit imageSizeChanged(lines, columns);
        }
    } else {
        _screen[0]->resizeImage(lines, columns);
        _screen[1]->resizeImage(lines, columns);

        emit imageSizeChanged(lines, columns);

        bufferedUpdate();
    }

    if (!_imageSizeInitialized) {
        _imageSizeInitialized = true;

        emit imageSizeInitialized();
    }
}

QSize Emulation::imageSize() const
{
    return {_currentScreen->getColumns(), _currentScreen->getLines()};
}
