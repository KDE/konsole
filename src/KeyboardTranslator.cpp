/*
    This source file is part of Konsole, a terminal emulator.

    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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
#include "KeyboardTranslator.h"

// System
#include <stdio.h>

// Qt
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtGui/QKeySequence>
#include <QtCore/QTextStream>
#include <QtCore/QDebug>

// KDE
#include <KStandardDirs>

using namespace Konsole;

KeyboardTranslatorManager* KeyboardTranslatorManager::_instance = 0;

KeyboardTranslatorManager::KeyboardTranslatorManager()
    : _haveLoadedAll(false)
{
}

QString KeyboardTranslatorManager::findTranslatorPath(const QString& name)
{
    return KGlobal::dirs()->findResource("data","konsole/"+name+".keytab");
}
void KeyboardTranslatorManager::findTranslators()
{
    QStringList list = KGlobal::dirs()->findAllResources("data","konsole/*.keytab");

    qDebug() << __FUNCTION__ << ": found " << list.count() << " keyboard translators.";

    // add the name of each translator to the list and associated
    // the name with a null pointer to indicate that the translator
    // has not yet been loaded from disk
    QStringListIterator listIter(list);
    while (listIter.hasNext())
    {
        QString translatorPath = listIter.next();

        QString name = QFileInfo(translatorPath).baseName();
       
        if ( !_translators.contains(name) ) 
            _translators.insert(name,0);
    }

    _haveLoadedAll = true;
}

const KeyboardTranslator* KeyboardTranslatorManager::findTranslator(const QString& name)
{
    if ( _translators.contains(name) && _translators[name] != 0 )
        return _translators[name];

    KeyboardTranslator* translator = loadTranslator(name);
    _translators[name] = translator;

    return translator;
}

KeyboardTranslator* KeyboardTranslatorManager::loadTranslator(const QString& name)
{
    KeyboardTranslator* translator = new KeyboardTranslator(name);

    const QString& path = findTranslatorPath(name);

    QFile source(path); 
    source.open(QIODevice::ReadOnly);

    KeyboardTranslatorReader reader(&source);
    translator->setDescription( reader.description() );
    while ( reader.hasNextEntry() )
        translator->addEntry(reader.nextEntry());

    source.close();

    if ( !reader.parseError() )
    {
        return translator;
    }
    else
    {
        delete translator;
        return 0;
    }
}


// each line of the keyboard translation file is one of:
//
// - keyboard "name"
// - key KeySequence : "characters"
// - key KeySequence : CommandName
//
// KeySequence begins with the name of the key ( taken from the Qt::Key enum )
// and is followed by the keyboard modifiers and state flags ( with + or - in front
// of each modifier or flag to indicate whether it is required ).  All keyboard modifiers
// and flags are optional, if a particular modifier or state is not specified it is 
// assumed not to be a part of the sequence.  The key sequence may contain whitespace
//
// eg:  "key Up+Shift : scrollLineUp"
//      "key Next-Shift : "\E[6~"
//
// (lines containing only whitespace are ignored, parseLine assumes that comments have
// already been removed)
//

KeyboardTranslatorReader::KeyboardTranslatorReader( QIODevice* source )
    : _source(source)
    , _hasNext(false)
{
   // read input until we find the description
   while ( _description.isEmpty() && !source->atEnd() )
   {
        const QList<Token>& tokens = tokenize( QString(source->readLine()) );
   
        if ( !tokens.isEmpty() && tokens.first().type == Token::TitleKeyword )
        {
            _description = tokens[1].text;
        }
   }

   readNext();
}
void KeyboardTranslatorReader::readNext() 
{
    // find next entry
    while ( !_source->atEnd() )
    {
        const QList<Token>& tokens = tokenize( QString(_source->readLine()) );
        if ( !tokens.isEmpty() && tokens.first().type == Token::KeyKeyword )
        {
            KeyboardTranslator::State flags = KeyboardTranslator::NoState;
            KeyboardTranslator::State flagMask = KeyboardTranslator::NoState;
            Qt::KeyboardModifier modifiers = Qt::NoModifier;
            Qt::KeyboardModifier modifierMask = Qt::NoModifier;

            int keyCode = Qt::Key_unknown;

            decodeSequence(tokens[1].text.toLower(),
                           keyCode,
                           modifiers,
                           modifierMask,
                           flags,
                           flagMask); 

            KeyboardTranslator::Command command = KeyboardTranslator::NoCommand;
            QByteArray text;

            // get text or command
            if ( tokens[2].type == Token::OutputText )
            {
                text = tokens[2].text.toLocal8Bit();
            }
            else if ( tokens[2].type == Token::Command )
            {
                // identify command
                if ( tokens[2].text.compare("scrollpageup",Qt::CaseInsensitive) == 0 )
                    command = KeyboardTranslator::ScrollPageUpCommand;
                else if ( tokens[2].text.compare("scrollpagedown",Qt::CaseInsensitive) == 0 )
                    command = KeyboardTranslator::ScrollPageDownCommand;
                else if ( tokens[2].text.compare("scrolllineup",Qt::CaseInsensitive) == 0 )
                    command = KeyboardTranslator::ScrollLineUpCommand;
                else if ( tokens[2].text.compare("scrolllinedown",Qt::CaseInsensitive) == 0 )
                    command = KeyboardTranslator::ScrollLineDownCommand;
                else if ( tokens[2].text.compare("scrolllock",Qt::CaseInsensitive) == 0 )
                    command = KeyboardTranslator::ScrollLockCommand;
                else
                    qDebug() << "Command not understood:" << tokens[2].text;

            }

            KeyboardTranslator::Entry newEntry;
            newEntry.setKeyCode( keyCode );
            newEntry.setState( flags );
            newEntry.setStateMask( flagMask );
            newEntry.setModifiers( modifiers );
            newEntry.setModifierMask( modifierMask );
            newEntry.setText( text );
            newEntry.setCommand( command );

            _nextEntry = newEntry; 

            _hasNext = true;

            return;
        }
    } 

    _hasNext = false;
}

bool KeyboardTranslatorReader::decodeSequence(const QString& text,
                                              int& keyCode,
                                              Qt::KeyboardModifier& modifiers,
                                              Qt::KeyboardModifier& modifierMask,
                                              KeyboardTranslator::State& flags,
                                              KeyboardTranslator::State& flagMask)
{
    bool isWanted = true; 
    bool endOfItem = false;
    QString buffer;

    int tempModifiers = modifiers;
    int tempModifierMask = modifierMask;
    int tempFlags = flags;
    int tempFlagMask = flagMask;

    for ( int i = 0 ; i < text.count() ; i++ )
    {
        const QChar& ch = text[i];
        bool isLastLetter = ( i == text.count()-1 );

        endOfItem = true;

        if ( ch.isLetterOrNumber() )
        {
            endOfItem = false;

            buffer.append(ch);
        }
        else if ( ch == '+' )
           isWanted = true;
        else if ( ch == '-' )
           isWanted = false; 

        if ( (endOfItem || isLastLetter) && !buffer.isEmpty() )
        {
            qDebug() << "Parsing item: " << buffer;

            int itemModifier = 0;
            int itemKeyCode = 0;
            int itemFlag = 0;

            if ( parseAsModifier(buffer,itemModifier) )
            {
                tempModifierMask |= itemModifier;

                if ( isWanted )
                    tempModifiers |= itemModifier;
            }
            else if ( parseAsStateFlag(buffer,itemFlag) )
            {
                tempFlagMask |= itemFlag;

                if ( isWanted )
                    tempFlags |= itemFlag;
            }
            else if ( parseAsKeyCode(buffer,itemKeyCode) )
                keyCode = itemKeyCode;
            else
                qDebug() << "Unable to parse key binding item:" << buffer;

            buffer.clear();
        }
    } 

    modifiers = (Qt::KeyboardModifier)tempModifiers;
    modifierMask = (Qt::KeyboardModifier)tempModifierMask;
    flags = (KeyboardTranslator::State)tempFlags;
    flagMask = (KeyboardTranslator::State)tempFlagMask;

    return true;
}

bool KeyboardTranslatorReader::parseAsModifier(const QString& item , int& modifier)
{
    if ( item == "shift" )
        modifier = Qt::ShiftModifier;
    else if ( item == "ctrl" || item == "control" )
        modifier = Qt::ControlModifier;
    else if ( item == "alt" )
        modifier = Qt::AltModifier;
    else if ( item == "meta" )
        modifier = Qt::MetaModifier;
    else
        return false;

    return true;
}
bool KeyboardTranslatorReader::parseAsStateFlag(const QString& item , int& flag)
{
    if ( item == "appcukeys" )
        flag = KeyboardTranslator::CursorKeysState;
    else if ( item == "ansi" )
        flag = KeyboardTranslator::AnsiState;
    else if ( item == "newline" )
        flag = KeyboardTranslator::NewLineState;
    else if ( item == "appscreen" )
        flag = KeyboardTranslator::AlternateScreenState;
    else if ( item == "anymod" )
        flag = KeyboardTranslator::AnyModifierState;
    else
        return false;

    return true;
}
bool KeyboardTranslatorReader::parseAsKeyCode(const QString& item , int& keyCode)
{
    QKeySequence sequence = QKeySequence::fromString(item);
    if ( !sequence.isEmpty() )
    {
        keyCode = sequence[0];

        if ( sequence.count() > 1 )
        {
            qDebug() << "Unhandled key codes in sequence: " << item;
        }
    }
    // additional cases implemented for backwards compatibility with KDE 3
    else if ( item == "prior" )
        keyCode = Qt::Key_PageUp;
    else if ( item == "next" )
        keyCode = Qt::Key_PageDown;
    else
        return false;

    return true;
}

#if 0
QKeySequence KeyboardTranslatorReader::decodeSequence(const QString& text , KeyboardTranslator::State& stateFlags )
{
    int state = KeyboardTranslator::NoState;
    stateFlags = (KeyboardTranslator::State)state;

    QKeySequence sequence = QKeySequence::fromString(text);

    if ( sequence.count() > 0 )
       return sequence;

    // if the sequence is empty, it failed to decode properly
    // this can happen if the file was produced by a KDE 3 version
    // of Konsole
    // 
    // possible reasons:
    //      - Key sequence includes names not known to Qt
    //        ( eg "Ansi" , "NewLine" )
    //      - Key sequence has modifiers at the end of the line ( eg "F10+Shift" )
    //        instead of at the start, which QKeySequence requires
    //      - Use of '-' in front of modifiers to indicate that they are 
    //        not required

    int modifiers = Qt::NoModifier;

    // first pass
    // rearrange modifers and try decoding again
    if ( text.contains("+shift",Qt::CaseInsensitive) )
        modifiers |= Qt::ShiftModifier;
    if ( text.contains("+ctrl",Qt::CaseInsensitive) )
        modifiers |= Qt::ControlModifier;
    if ( text.contains("+control",Qt::CaseInsensitive) )
        modifiers |= Qt::ControlModifier;
    if ( text.contains("+alt",Qt::CaseInsensitive) )
        modifiers |= Qt::AltModifier;
    if ( text.contains("+meta",Qt::CaseInsensitive) )
        modifiers |= Qt::MetaModifier;
    if ( text.contains("+ansi",Qt::CaseInsensitive) )
        state |= KeyboardTranslator::AnsiState;
    if ( text.contains("+appcukeys",Qt::CaseInsensitive) )
        state |= KeyboardTranslator::CursorKeysState;
    if ( text.contains("+newline",Qt::CaseInsensitive) )
        state |= KeyboardTranslator::NewLineState;

    static QRegExp modifierRemover("((\\+|\\-)shift|"
                                   "(\\+|\\-)ctrl|"
                                   "(\\+|\\-)control|"
                                   "(\\+|\\-)alt|"
                                   "(\\+|\\-)ansi|"
                                   "(\\+|\\-)appcukeys|"
                                   "(\\+|\\-)newline|"
                                   "(\\+|\\-)meta)",Qt::CaseInsensitive);
    QString newText(text);
    newText.replace(modifierRemover,QString());

    if ( modifiers & Qt::ShiftModifier )
        newText.prepend("shift+");
    if ( modifiers & Qt::ControlModifier )
        newText.prepend("ctrl+");
    if ( modifiers & Qt::AltModifier )
        newText.prepend("alt+");
    if ( modifiers & Qt::MetaModifier )
        newText.prepend("meta+");

    //qDebug() << "modifier: " << newText;
    sequence = QKeySequence::fromString(newText);
    //qDebug() << "after: " << sequence;

    stateFlags = (KeyboardTranslator::State)state;

    return sequence;
}
#endif 

QString KeyboardTranslatorReader::description() const
{
    return _description;
}
bool KeyboardTranslatorReader::hasNextEntry()
{
    return _hasNext;
}
KeyboardTranslator::Entry KeyboardTranslatorReader::nextEntry() 
{
    Q_ASSERT( _hasNext );


    KeyboardTranslator::Entry entry = _nextEntry;

    readNext();

    return entry;
}
bool KeyboardTranslatorReader::parseError()
{
    return false;
}
QList<KeyboardTranslatorReader::Token> KeyboardTranslatorReader::tokenize(const QString& line)
{
    QString text = line.simplified();

    // comment line: # comment
    static QRegExp comment("\\#.*");
    // title line: keyboard "title"
    static QRegExp title("keyboard\\s+\"(.*)\"");
    // key line: key KeySequence : "output"
    // key line: key KeySequence : command
    static QRegExp key("key\\s+([\\w\\+\\s\\-]+)\\s*:\\s*(\"(.*)\"|\\w+)");

    QList<Token> list;

   // qDebug() << "line: " << text;

    if ( text.isEmpty() || comment.exactMatch(text) )
    {
     //   qDebug() << "This is a comment";
        return list;
    }

    if ( title.exactMatch(text) )
    {
        Token titleToken = { Token::TitleKeyword , QString::null };
        Token textToken = { Token::TitleText , title.capturedTexts()[1] };
    
        list << titleToken << textToken;
    }
    else if  ( key.exactMatch(text) )
    {
        Token keyToken = { Token::KeyKeyword , QString::null };
        Token sequenceToken = { Token::KeySequence , key.capturedTexts()[1].remove(' ') };

        list << keyToken << sequenceToken;

        if ( key.capturedTexts()[3].isEmpty() )
        {
            // capturedTexts()[2] is a command
            Token commandToken = { Token::Command , key.capturedTexts()[2] };
            list << commandToken;    
        }   
        else
        {
            // capturedTexts()[3] is the output string
           Token outputToken = { Token::OutputText , key.capturedTexts()[3] };
           list << outputToken;
        }     
    }
    else
    {
        qWarning() << "Line in keyboard translator file could not be understood:" << text;
    }

    return list;
}

QList<QString> KeyboardTranslatorManager::allTranslators() 
{
    if ( !_haveLoadedAll )
    {
        findTranslators();
    }

    return _translators.keys();
}

KeyboardTranslator::Entry::Entry()
: _keyCode(0)
, _modifiers(Qt::NoModifier)
, _state(NoState)
, _command(NoCommand)
{
}

#if 0
QKeySequence KeyboardTranslator::Entry::keySequence() const
{
    int code = _keyCode;

    if ( _modifiers & Qt::AltModifier )
        code += Qt::ALT;
    if ( _modifiers & Qt::ControlModifier )
        code += Qt::CTRL;
    if ( _modifiers & Qt::ShiftModifier )
        code += Qt::SHIFT;
    if ( _modifiers & Qt::MetaModifier )
        code += Qt::META;

    return QKeySequence(code);
}
#endif 

bool KeyboardTranslator::Entry::matches(int keyCode , Qt::KeyboardModifier modifiers,
                                        State state) const
{
    if ( _keyCode != keyCode )
        return false;

    if ( (modifiers & _modifierMask) != (_modifiers & _modifierMask) ) 
        return false;

    if ( (state & _stateMask) != (_state & _stateMask) )
        return false;

    // special handling for the 'Any Modifier' state, which checks for the presence of 
    // any or no modifiers 
    if ( _stateMask & KeyboardTranslator::AnyModifierState )
    {
        // test fails if any modifier is required but none are set
        if ( (_state & KeyboardTranslator::AnyModifierState) && modifiers == 0 )
           return false;

        // test fails if no modifier is allowed but one or more are set
        if ( !(_state & KeyboardTranslator::AnyModifierState) && modifiers != 0 )
            return false;
    }

    return true;
}
void KeyboardTranslator::Entry::insertModifier( QString& item , int modifier ) const
{
    if ( !(modifier & _modifierMask) )
        return;

    if ( modifier & _modifiers )
        item += '+';
    else
        item += '-';

    if ( modifier == Qt::ShiftModifier )
        item += "Shift";
    else if ( modifier == Qt::ControlModifier )
        item += "Ctrl";
    else if ( modifier == Qt::AltModifier )
        item += "Alt";
    else if ( modifier == Qt::MetaModifier )
        item += "Meta";
}
void KeyboardTranslator::Entry::insertState( QString& item , int state ) const
{
    if ( !(state & _stateMask) )
        return;

    if ( state & _state )
        item += '+' ;
    else
        item += '-' ;

    if ( state == KeyboardTranslator::AlternateScreenState )
        item += "AppScreen";
    else if ( state == KeyboardTranslator::NewLineState )
        item += "NewLine";
    else if ( state == KeyboardTranslator::AnsiState )
        item += "ansi";
    else if ( state == KeyboardTranslator::CursorKeysState )
        item += "AppCuKeys";
    else if ( state == KeyboardTranslator::AnyModifierState )
        item += "AnyMod";
}
QString KeyboardTranslator::Entry::conditionToString() const
{
    QString result = QKeySequence(_keyCode).toString();

    // add modifiers
    insertModifier( result , Qt::ShiftModifier );
    insertModifier( result , Qt::ControlModifier );
    insertModifier( result , Qt::AltModifier );
    insertModifier( result , Qt::MetaModifier ); 

    // add states
    insertState( result , KeyboardTranslator::AlternateScreenState );
    insertState( result , KeyboardTranslator::NewLineState );
    insertState( result , KeyboardTranslator::AnsiState );
    insertState( result , KeyboardTranslator::CursorKeysState );
    insertState( result , KeyboardTranslator::AnyModifierState );

    return result;
}

KeyboardTranslator::KeyboardTranslator(const QString& name)
: _name(name)
{
}
KeyboardTranslator::KeyboardTranslator(const KeyboardTranslator& other)
: _name(other._name)
{
    _description = other._description;

    // TODO: Copy keyboard entries
}
void KeyboardTranslator::setDescription(const QString& description) 
{
    _description = description;
}
QString KeyboardTranslator::description() const
{
    return _description;
}
void KeyboardTranslator::setName(const QString& name)
{
    _name = name;
}
QString KeyboardTranslator::name() const
{
    return _name;
}

QList<KeyboardTranslator::Entry> KeyboardTranslator::entries() const
{
    return _entries.values();
}

void KeyboardTranslator::addEntry(const Entry& entry)
{
    const int keyCode = entry.keyCode();
    _entries.insert(keyCode,entry);
}

KeyboardTranslator::Entry KeyboardTranslator::findEntry(int keyCode, Qt::KeyboardModifier modifiers, State state) const
{
    if ( _entries.contains(keyCode) )
    {
        QListIterator<Entry> iter(_entries.values(keyCode));

        while (iter.hasNext())
        {
            const Entry& next = iter.next();
            if ( next.matches(keyCode,modifiers,state) )
                return next;
        }

        return Entry(); // entry not found
    }
    else
    {
        return Entry();
    }
}
void KeyboardTranslatorManager::addTranslator(KeyboardTranslator* translator)
{
    qWarning() << __FUNCTION__ << ": Not implemented";
}
void KeyboardTranslatorManager::deleteTranslator(const QString& name)
{
    qWarning() << __FUNCTION__ << ": Not implemented";
}
void KeyboardTranslatorManager::setInstance(KeyboardTranslatorManager* instance)
{
    _instance = instance;
}
KeyboardTranslatorManager* KeyboardTranslatorManager::instance()
{
    return _instance;
}
