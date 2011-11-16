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

// System
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Qt
#include <QtGui/QApplication>
#include <QtGui/QClipboard>
#include <QtCore/QHash>
#include <QtGui/QKeyEvent>
#include <QtCore/QRegExp>
#include <QtCore/QTextStream>
#include <QtCore/QThread>
#include <QtCore/QTime>

// KDE
#include <KDebug>

// Konsole
#include "KeyboardTranslator.h"
#include "Screen.h"
#include "TerminalCharacterDecoder.h"
#include "ScreenWindow.h"
#include "SessionManager.h"
#include "Session.h"
#include "TerminalDisplay.h"

using namespace Konsole;

Emulation::Emulation() :
  _currentScreen(0),
  _codec(0),
  _decoder(0),
  _keyTranslator(0),
  _usesMouse(false),
  _imageSizeInitialized(false)
{
    // create screens with a default size
    _screen[0] = new Screen(40,80);
    _screen[1] = new Screen(40,80);
    _currentScreen = _screen[0];

    QObject::connect(&_bulkTimer1, SIGNAL(timeout()), this, SLOT(showBulk()) );
    QObject::connect(&_bulkTimer2, SIGNAL(timeout()), this, SLOT(showBulk()) );

    // listen for mouse status changes
    connect( this , SIGNAL(programUsesMouseChanged(bool)) , 
            SLOT(usesMouseChanged(bool)) );
}

bool Emulation::programUsesMouse() const
{
    return _usesMouse;
}

void Emulation::usesMouseChanged(bool usesMouse)
{
    _usesMouse = usesMouse;
}

ScreenWindow* Emulation::createWindow()
{
    ScreenWindow* window = new ScreenWindow();
    window->setScreen(_currentScreen);
    _windows << window;

    connect(window , SIGNAL(selectionChanged()),
            this , SLOT(bufferedUpdate()));
    connect(window, SIGNAL(selectionChanged()),
            this, SLOT(checkSelectedText()));

    connect(this , SIGNAL(outputChanged()),
            window , SLOT(notifyOutputChanged()) );

    return window;
}

void Emulation::checkScreenInUse()
{
    emit primaryScreenInUse( _currentScreen == _screen[0] );
}

void Emulation::checkSelectedText()
{
    QString text = _currentScreen->selectedText(true);
    emit selectedText(text);
}

Emulation::~Emulation()
{
    QListIterator<ScreenWindow*> windowIter(_windows);

    while (windowIter.hasNext())
    {
        delete windowIter.next();
    }

    delete _screen[0];
    delete _screen[1];
    delete _decoder;
}

void Emulation::setScreen(int n)
{
    Screen* old = _currentScreen;
    _currentScreen = _screen[n & 1];
    if (_currentScreen != old) 
    {
        // tell all windows onto this emulation to switch to the newly active screen
        foreach(ScreenWindow* window,_windows)
            window->setScreen(_currentScreen);

        checkScreenInUse();
        checkSelectedText();
    }
}

void Emulation::clearHistory()
{
    _screen[0]->setScroll( _screen[0]->getScroll() , false );
}
void Emulation::setHistory(const HistoryType& t)
{
    _screen[0]->setScroll(t);

    showBulk();
}

const HistoryType& Emulation::history() const
{
    return _screen[0]->getScroll();
}

void Emulation::setCodec(const QTextCodec * codec)
{
    if ( codec )
    {
        _codec = codec;

        delete _decoder;
        _decoder = _codec->makeDecoder();

        emit useUtf8Request(utf8());
    }
    else
    {
        setCodec(LocaleCodec);
    }
}

void Emulation::setCodec(EmulationCodec codec)
{
    if ( codec == Utf8Codec )
        setCodec( QTextCodec::codecForName("utf8") );
    else if ( codec == LocaleCodec )
        setCodec( QTextCodec::codecForLocale() );
}

void Emulation::setKeyBindings(const QString& name)
{
    _keyTranslator = KeyboardTranslatorManager::instance()->findTranslator(name);
    if (!_keyTranslator)
    {
        _keyTranslator = KeyboardTranslatorManager::instance()->defaultTranslator();
    }
}

QString Emulation::keyBindings() const
{
    return _keyTranslator->name();
}

// process application unicode input to terminal
// this is a trivial scanner
void Emulation::receiveChar(int c)
{
    c &= 0xff;
    switch (c)
    {
        case '\b'      : _currentScreen->backspace();                 break;
        case '\t'      : _currentScreen->tab();                       break;
        case '\n'      : _currentScreen->newLine();                   break;
        case '\r'      : _currentScreen->toStartOfLine();             break;
        case 0x07      : emit stateSet(NOTIFYBELL);                   break;
        default        : _currentScreen->displayCharacter(c);         break;
    };
}

void Emulation::sendKeyEvent( QKeyEvent* ev )
{
    emit stateSet(NOTIFYNORMAL);

    if (!ev->text().isEmpty())
    {
        // A block of text
        // Note that the text is proper unicode.
        // We should do a conversion here
        emit sendData(ev->text().toUtf8(),ev->text().length());
    }
}

void Emulation::sendString(const char*,int)
{
    // default implementation does nothing
}

void Emulation::sendMouseEvent(int /*buttons*/, int /*column*/, int /*row*/, int /*eventType*/)
{
    // default implementation does nothing
}

/*
   We are doing code conversion from locale to unicode first.
*/

void Emulation::receiveData(const char* text, int length)
{
    emit stateSet(NOTIFYACTIVITY);

    bufferedUpdate();

    QString unicodeText = _decoder->toUnicode(text,length);

    //send characters to terminal emulator
    for (int i=0;i<unicodeText.length();i++)
        receiveChar(unicodeText[i].unicode());

    //look for z-modem indicator
    //-- someone who understands more about z-modems that I do may be able to move
    //this check into the above for loop?
    for (int i=0;i<length;i++)
    {
        if (text[i] == '\030')
        {
            if ((length-i-1 > 3) && (strncmp(text+i+1, "B00", 3) == 0))
                emit zmodemDetected();
        }
    }
}

//OLDER VERSION
//This version of onRcvBlock was commented out because
//    a)  It decoded incoming characters one-by-one, which is slow in the current version of Qt (4.2 tech preview)
//    b)  It messed up decoding of non-ASCII characters, with the result that (for example) chinese characters
//        were not printed properly.
//
//There is something about stopping the _decoder if "we get a control code halfway a multi-byte sequence" (see below)
//which hasn't been ported into the newer function (above).  Hopefully someone who understands this better
//can find an alternative way of handling the check.  


/*void Emulation::onRcvBlock(const char *s, int len)
{
  emit notifySessionState(NOTIFYACTIVITY);

  bufferedUpdate();
  for (int i = 0; i < len; i++)
  {

    QString result = _decoder->toUnicode(&s[i],1);
    int reslen = result.length();

    // If we get a control code halfway a multi-byte sequence
    // we flush the _decoder and continue with the control code.
    if ((s[i] < 32) && (s[i] > 0))
    {
       // Flush _decoder
       while(!result.length())
          result = _decoder->toUnicode(&s[i],1);
       reslen = 1;
       result.resize(reslen);
       result[0] = QChar(s[i]);
    }

    for (int j = 0; j < reslen; j++)
    {
      if (result[j].characterategory() == QChar::Mark_NonSpacing)
         _currentScreen->compose(result.mid(j,1));
      else
         onRcvChar(result[j].unicode());
    }
    if (s[i] == '\030')
    {
      if ((len-i-1 > 3) && (strncmp(s+i+1, "B00", 3) == 0))
          emit zmodemDetected();
    }
  }
}*/

void Emulation::writeToStream( TerminalCharacterDecoder* decoder , 
                               int startLine ,
                               int endLine) 
{
    _currentScreen->writeLinesToStream(decoder,startLine,endLine);
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
    if (!_bulkTimer2.isActive())
    {
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
    if ((lines < 1) || (columns < 1))
        return;

    QSize screenSize[2] = { QSize(_screen[0]->getColumns(),
                                  _screen[0]->getLines()),
                            QSize(_screen[1]->getColumns(),
                                  _screen[1]->getLines()) };
    QSize newSize(columns,lines);

    if (newSize == screenSize[0] && newSize == screenSize[1])
    {
        // If this method is called for the first time, always emit
        // SIGNAL(imageSizeChange()), even if the new size is the same as the
        // current size.  See #176902
        if (!_imageSizeInitialized)
        {
            emit imageSizeChanged(lines,columns);
        }
    }
    else
    {
        _screen[0]->resizeImage(lines,columns);
        _screen[1]->resizeImage(lines,columns);

        emit imageSizeChanged(lines,columns);

        bufferedUpdate();
    }

    if (!_imageSizeInitialized)
    {
        _imageSizeInitialized = true;

        // FIXME
        // a hard-coded, small delay is introduced to guarantee Session::run()
        // does not get triggered by SIGNAL(imageSizeInitialized()) before
        // Pty::setWindowSize() is triggered by previously emitted
        // SIGNAL(imageSizeChanged()); See #203185
        QTimer::singleShot(200, this, SIGNAL(imageSizeInitialized()) );
    }

}

QSize Emulation::imageSize() const
{
    return QSize(_currentScreen->getColumns(), _currentScreen->getLines());
}

ushort ExtendedCharTable::extendedCharHash(const ushort* unicodePoints , ushort length) const
{
    ushort hash = 0;
    for ( ushort i = 0 ; i < length ; i++ )
    {
        hash = 31*hash + unicodePoints[i];
    }
    return hash;
}
bool ExtendedCharTable::extendedCharMatch(ushort hash , const ushort* unicodePoints , ushort length) const
{
    ushort* entry = extendedCharTable[hash];

    // compare given length with stored sequence length ( given as the first ushort in the 
    // stored buffer ) 
    if ( entry == 0 || entry[0] != length ) 
       return false;
    // if the lengths match, each character must be checked.  the stored buffer starts at
    // entry[1]
    for ( int i = 0 ; i < length ; i++ )
    {
        if ( entry[i+1] != unicodePoints[i] )
           return false; 
    } 
    return true;
}
ushort ExtendedCharTable::createExtendedChar(const ushort* unicodePoints , ushort length)
{
    // look for this sequence of points in the table
    ushort hash = extendedCharHash(unicodePoints,length);
    const ushort initialHash = hash;
    bool triedCleaningSolution = false;

    // check existing entry for match
    while ( extendedCharTable.contains(hash) && hash != 0 ) // 0 has a special meaning for chars so we don't use it
    {
        if ( extendedCharMatch(hash, unicodePoints, length) )
        {
            // this sequence already has an entry in the table, 
            // return its hash
            return hash;
        }
        else
        {
            // if hash is already used by another, different sequence of unicode character
            // points then try next hash
            hash++;

            if (hash == initialHash)
            {
                if (!triedCleaningSolution)
                {
                    triedCleaningSolution = true;
                    // All the hashes are full, go to all Screens and try to free any
                    // This is slow but should happen very rarely
                    QSet<ushort> usedExtendedChars;
                    const SessionManager* sm = SessionManager::instance();
                    foreach(const Session* s, sm->sessions())
                    {
                        foreach(const TerminalDisplay* td, s->views())
                        {
                            usedExtendedChars += td->screenWindow()->screen()->usedExtendedChars();

                        }
                    }

                    QHash<ushort,ushort*>::iterator it = extendedCharTable.begin();
                    QHash<ushort,ushort*>::iterator itEnd = extendedCharTable.end();
                    while (it != itEnd)
                    {
                        if (usedExtendedChars.contains(it.key()))
                        {
                            ++it;
                        }
                        else
                        {
                            it = extendedCharTable.erase(it);
                        }
                    }
                }
                else
                {
                    kWarning() << "Using all the extended char hashes, going to miss this extended character";
                    return 0;
                }
            }
        }
    }    


    // add the new sequence to the table and
    // return that index
    ushort* buffer = new ushort[length+1];
    buffer[0] = length;
    for ( int i = 0 ; i < length ; i++ )
       buffer[i+1] = unicodePoints[i]; 

    extendedCharTable.insert(hash,buffer);

    return hash;
}

ushort* ExtendedCharTable::lookupExtendedChar(ushort hash , ushort& length) const
{
    // lookup index in table and if found, set the length
    // argument and return a pointer to the character sequence

    ushort* buffer = extendedCharTable[hash];
    if ( buffer )
    {
        length = buffer[0];
        return buffer+1;
    }
    else
    {
        length = 0;
        return 0;
    }
}

ExtendedCharTable::ExtendedCharTable()
{
}
ExtendedCharTable::~ExtendedCharTable()
{
    // free all allocated character buffers
    QHashIterator<ushort,ushort*> iter(extendedCharTable);
    while ( iter.hasNext() )
    {
        iter.next();
        delete[] iter.value();
    }
}

// global instance
ExtendedCharTable ExtendedCharTable::instance;


#include "Emulation.moc"

