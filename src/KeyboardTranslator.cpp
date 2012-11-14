/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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
#include <ctype.h>
#include <stdio.h>

// Qt
#include <QtCore/QBuffer>
#include <QtCore/QTextStream>
#include <QtGui/QKeySequence>

// KDE
#include <KDebug>
#include <KLocalizedString>

using namespace Konsole;

KeyboardTranslatorWriter::KeyboardTranslatorWriter(QIODevice* destination)
    : _destination(destination)
{
    Q_ASSERT(destination && destination->isWritable());

    _writer = new QTextStream(_destination);
}
KeyboardTranslatorWriter::~KeyboardTranslatorWriter()
{
    delete _writer;
}
void KeyboardTranslatorWriter::writeHeader(const QString& description)
{
    *_writer << "keyboard \"" << description << '\"' << '\n';
}
void KeyboardTranslatorWriter::writeEntry(const KeyboardTranslator::Entry& entry)
{
    QString result;
    if (entry.command() != KeyboardTranslator::NoCommand)
        result = entry.resultToString();
    else
        result = '\"' + entry.resultToString() + '\"';

    *_writer << "key " << entry.conditionToString() << " : " << result << '\n';
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
//      "key PgDown-Shift : "\E[6~"
//
// (lines containing only whitespace are ignored, parseLine assumes that comments have
// already been removed)
//

KeyboardTranslatorReader::KeyboardTranslatorReader(QIODevice* source)
    : _source(source)
    , _hasNext(false)
{
    // read input until we find the description
    while (_description.isEmpty() && !source->atEnd()) {
        QList<Token> tokens = tokenize(QString::fromLocal8Bit(source->readLine()));
        if (!tokens.isEmpty() && tokens.first().type == Token::TitleKeyword)
            _description = i18n(tokens[1].text.toUtf8());
    }
    // read first entry (if any)
    readNext();
}
void KeyboardTranslatorReader::readNext()
{
    // find next entry
    while (!_source->atEnd()) {
        const QList<Token>& tokens = tokenize(QString::fromLocal8Bit(_source->readLine()));
        if (!tokens.isEmpty() && tokens.first().type == Token::KeyKeyword) {
            KeyboardTranslator::States flags = KeyboardTranslator::NoState;
            KeyboardTranslator::States flagMask = KeyboardTranslator::NoState;
            Qt::KeyboardModifiers modifiers = Qt::NoModifier;
            Qt::KeyboardModifiers modifierMask = Qt::NoModifier;

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
            if (tokens[2].type == Token::OutputText) {
                text = tokens[2].text.toLocal8Bit();
            } else if (tokens[2].type == Token::Command) {
                // identify command
                if (!parseAsCommand(tokens[2].text, command))
                    kWarning() << "Key" << tokens[1].text << ", Command" << tokens[2].text << "not understood. ";
            }

            KeyboardTranslator::Entry newEntry;
            newEntry.setKeyCode(keyCode);
            newEntry.setState(flags);
            newEntry.setStateMask(flagMask);
            newEntry.setModifiers(modifiers);
            newEntry.setModifierMask(modifierMask);
            newEntry.setText(text);
            newEntry.setCommand(command);

            _nextEntry = newEntry;

            _hasNext = true;

            return;
        }
    }

    _hasNext = false;
}

bool KeyboardTranslatorReader::parseAsCommand(const QString& text, KeyboardTranslator::Command& command)
{
    if (text.compare("erase", Qt::CaseInsensitive) == 0)
        command = KeyboardTranslator::EraseCommand;
    else if (text.compare("scrollpageup", Qt::CaseInsensitive) == 0)
        command = KeyboardTranslator::ScrollPageUpCommand;
    else if (text.compare("scrollpagedown", Qt::CaseInsensitive) == 0)
        command = KeyboardTranslator::ScrollPageDownCommand;
    else if (text.compare("scrolllineup", Qt::CaseInsensitive) == 0)
        command = KeyboardTranslator::ScrollLineUpCommand;
    else if (text.compare("scrolllinedown", Qt::CaseInsensitive) == 0)
        command = KeyboardTranslator::ScrollLineDownCommand;
    else if (text.compare("scrolluptotop", Qt::CaseInsensitive) == 0)
        command = KeyboardTranslator::ScrollUpToTopCommand;
    else if (text.compare("scrolldowntobottom", Qt::CaseInsensitive) == 0)
        command = KeyboardTranslator::ScrollDownToBottomCommand;
    else
        return false;

    return true;
}

bool KeyboardTranslatorReader::decodeSequence(const QString& text,
        int& keyCode,
        Qt::KeyboardModifiers& modifiers,
        Qt::KeyboardModifiers& modifierMask,
        KeyboardTranslator::States& flags,
        KeyboardTranslator::States& flagMask)
{
    bool isWanted = true;
    bool endOfItem = false;
    QString buffer;

    Qt::KeyboardModifiers tempModifiers = modifiers;
    Qt::KeyboardModifiers tempModifierMask = modifierMask;
    KeyboardTranslator::States tempFlags = flags;
    KeyboardTranslator::States tempFlagMask = flagMask;

    for (int i = 0 ; i < text.count() ; i++) {
        const QChar& ch = text[i];
        const bool isFirstLetter = (i == 0);
        const bool isLastLetter = (i == text.count() - 1);
        endOfItem = true;
        if (ch.isLetterOrNumber()) {
            endOfItem = false;
            buffer.append(ch);
        } else if (isFirstLetter) {
            buffer.append(ch);
        }

        if ((endOfItem || isLastLetter) && !buffer.isEmpty()) {
            Qt::KeyboardModifier itemModifier = Qt::NoModifier;
            int itemKeyCode = 0;
            KeyboardTranslator::State itemFlag = KeyboardTranslator::NoState;

            if (parseAsModifier(buffer, itemModifier)) {
                tempModifierMask |= itemModifier;

                if (isWanted)
                    tempModifiers |= itemModifier;
            } else if (parseAsStateFlag(buffer, itemFlag)) {
                tempFlagMask |= itemFlag;

                if (isWanted)
                    tempFlags |= itemFlag;
            } else if (parseAsKeyCode(buffer, itemKeyCode))
                keyCode = itemKeyCode;
            else
                kWarning() << "Unable to parse key binding item:" << buffer;

            buffer.clear();
        }

        // check if this is a wanted / not-wanted flag and update the
        // state ready for the next item
        if (ch == '+')
            isWanted = true;
        else if (ch == '-')
            isWanted = false;
    }

    modifiers = tempModifiers;
    modifierMask = tempModifierMask;
    flags = tempFlags;
    flagMask = tempFlagMask;

    return true;
}

bool KeyboardTranslatorReader::parseAsModifier(const QString& item , Qt::KeyboardModifier& modifier)
{
    if (item == "shift")
        modifier = Qt::ShiftModifier;
    else if (item == "ctrl" || item == "control")
        modifier = Qt::ControlModifier;
    else if (item == "alt")
        modifier = Qt::AltModifier;
    else if (item == "meta")
        modifier = Qt::MetaModifier;
    else if (item == "keypad")
        modifier = Qt::KeypadModifier;
    else
        return false;

    return true;
}
bool KeyboardTranslatorReader::parseAsStateFlag(const QString& item , KeyboardTranslator::State& flag)
{
    if (item == "appcukeys" || item == "appcursorkeys")
        flag = KeyboardTranslator::CursorKeysState;
    else if (item == "ansi")
        flag = KeyboardTranslator::AnsiState;
    else if (item == "newline")
        flag = KeyboardTranslator::NewLineState;
    else if (item == "appscreen")
        flag = KeyboardTranslator::AlternateScreenState;
    else if (item == "anymod" || item == "anymodifier")
        flag = KeyboardTranslator::AnyModifierState;
    else if (item == "appkeypad")
        flag = KeyboardTranslator::ApplicationKeypadState;
    else
        return false;

    return true;
}
bool KeyboardTranslatorReader::parseAsKeyCode(const QString& item , int& keyCode)
{
    QKeySequence sequence = QKeySequence::fromString(item);
    if (!sequence.isEmpty()) {
        keyCode = sequence[0];

        if (sequence.count() > 1) {
            kWarning() << "Unhandled key codes in sequence: " << item;
        }
        // additional cases implemented for backwards compatibility with KDE 3
    } else if (item == "prior")  // TODO: remove it in the future
        keyCode = Qt::Key_PageUp;
    else if (item == "next")     // TODO: remove it in the future
        keyCode = Qt::Key_PageDown;
    else
        return false;

    return true;
}

QString KeyboardTranslatorReader::description() const
{
    return _description;
}
bool KeyboardTranslatorReader::hasNextEntry()
{
    return _hasNext;
}
KeyboardTranslator::Entry KeyboardTranslatorReader::createEntry(const QString& condition ,
        const QString& result)
{
    QString entryString("keyboard \"temporary\"\nkey ");
    entryString.append(condition);
    entryString.append(" : ");

    // if 'result' is the name of a command then the entry result will be that command,
    // otherwise the result will be treated as a string to echo when the key sequence
    // specified by 'condition' is pressed
    KeyboardTranslator::Command command;
    if (parseAsCommand(result, command))
        entryString.append(result);
    else
        entryString.append('\"' + result + '\"');

    QByteArray array = entryString.toUtf8();
    QBuffer buffer(&array);
    buffer.open(QIODevice::ReadOnly);
    KeyboardTranslatorReader reader(&buffer);

    KeyboardTranslator::Entry entry;
    if (reader.hasNextEntry())
        entry = reader.nextEntry();

    return entry;
}

KeyboardTranslator::Entry KeyboardTranslatorReader::nextEntry()
{
    Q_ASSERT(_hasNext);
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
    QString text = line;

    // remove comments
    bool inQuotes = false;
    int commentPos = -1;
    for (int i = text.length() - 1; i >= 0; i--) {
        QChar ch = text[i];
        if (ch == '\"')
            inQuotes = !inQuotes;
        else if (ch == '#' && !inQuotes)
            commentPos = i;
    }
    if (commentPos != -1)
        text.remove(commentPos, text.length());

    text = text.simplified();

    // title line: keyboard "title"
    static QRegExp title("keyboard\\s+\"(.*)\"");
    // key line: key KeySequence : "output"
    // key line: key KeySequence : command
    static QRegExp key("key\\s+([\\w\\+\\s\\-\\*\\.]+)\\s*:\\s*(\"(.*)\"|\\w+)");

    QList<Token> list;
    if (text.isEmpty()) {
        return list;
    }

    if (title.exactMatch(text)) {
        Token titleToken = { Token::TitleKeyword , QString() };
        Token textToken = { Token::TitleText , title.capturedTexts()[1] };

        list << titleToken << textToken;
    } else if (key.exactMatch(text)) {
        Token keyToken = { Token::KeyKeyword , QString() };
        Token sequenceToken = { Token::KeySequence , key.capturedTexts()[1].remove(' ') };

        list << keyToken << sequenceToken;

        if (key.capturedTexts()[3].isEmpty()) {
            // capturedTexts()[2] is a command
            Token commandToken = { Token::Command , key.capturedTexts()[2] };
            list << commandToken;
        } else {
            // capturedTexts()[3] is the output string
            Token outputToken = { Token::OutputText , key.capturedTexts()[3] };
            list << outputToken;
        }
    } else {
        kWarning() << "Line in keyboard translator file could not be understood:" << text;
    }

    return list;
}

KeyboardTranslator::Entry::Entry()
    : _keyCode(0)
    , _modifiers(Qt::NoModifier)
    , _modifierMask(Qt::NoModifier)
    , _state(NoState)
    , _stateMask(NoState)
    , _command(NoCommand)
{
}

bool KeyboardTranslator::Entry::operator==(const Entry& rhs) const
{
    return _keyCode == rhs._keyCode &&
           _modifiers == rhs._modifiers &&
           _modifierMask == rhs._modifierMask &&
           _state == rhs._state &&
           _stateMask == rhs._stateMask &&
           _command == rhs._command &&
           _text == rhs._text;
}

bool KeyboardTranslator::Entry::matches(int testKeyCode,
                                        Qt::KeyboardModifiers testKeyboardModifiers,
                                        States testState) const
{
    if (_keyCode != testKeyCode)
        return false;

    if ((testKeyboardModifiers & _modifierMask) != (_modifiers & _modifierMask))
        return false;

    // if testKeyboardModifiers is non-zero, the 'any modifier' state is implicit
    if (testKeyboardModifiers != 0)
        testState |= AnyModifierState;

    if ((testState & _stateMask) != (_state & _stateMask))
        return false;

    // special handling for the 'Any Modifier' state, which checks for the presence of
    // any or no modifiers.  In this context, the 'keypad' modifier does not count.
    bool anyModifiersSet = (testKeyboardModifiers != 0)
                           && (testKeyboardModifiers != Qt::KeypadModifier);
    bool wantAnyModifier = _state & KeyboardTranslator::AnyModifierState;
    if (_stateMask & KeyboardTranslator::AnyModifierState) {
        if (wantAnyModifier != anyModifiersSet)
            return false;
    }

    return true;
}
QByteArray KeyboardTranslator::Entry::escapedText(bool expandWildCards,
        Qt::KeyboardModifiers keyboardModifiers) const
{
    QByteArray result(text(expandWildCards, keyboardModifiers));

    for (int i = 0 ; i < result.count() ; i++) {
        const char ch = result[i];
        char replacement = 0;

        switch (ch) {
        case 27 : replacement = 'E'; break;
        case 8  : replacement = 'b'; break;
        case 12 : replacement = 'f'; break;
        case 9  : replacement = 't'; break;
        case 13 : replacement = 'r'; break;
        case 10 : replacement = 'n'; break;
        default:
            // any character which is not printable is replaced by an equivalent
            // \xhh escape sequence (where 'hh' are the corresponding hex digits)
            if (!QChar(ch).isPrint())
                replacement = 'x';
        }

        if (replacement == 'x') {
            result.replace(i, 1, "\\x" + QByteArray(1, ch).toHex());
        } else if (replacement != 0) {
            result.remove(i, 1);
            result.insert(i, '\\');
            result.insert(i + 1, replacement);
        }
    }

    return result;
}
QByteArray KeyboardTranslator::Entry::unescape(const QByteArray& input) const
{
    QByteArray result(input);

    for (int i = 0 ; i < result.count() - 1 ; i++) {
        QByteRef ch = result[i];
        if (ch == '\\') {
            char replacement[2] = {0, 0};
            int charsToRemove = 2;
            bool escapedChar = true;

            switch (result[i + 1]) {
            case 'E' : replacement[0] = 27; break;
            case 'b' : replacement[0] = 8 ; break;
            case 'f' : replacement[0] = 12; break;
            case 't' : replacement[0] = 9 ; break;
            case 'r' : replacement[0] = 13; break;
            case 'n' : replacement[0] = 10; break;
            case 'x' : {
                // format is \xh or \xhh where 'h' is a hexadecimal
                // digit from 0-9 or A-F which should be replaced
                // with the corresponding character value
                char hexDigits[3] = {0};

                if ((i < result.count() - 2) && isxdigit(result[i + 2]))
                    hexDigits[0] = result[i + 2];
                if ((i < result.count() - 3) && isxdigit(result[i + 3]))
                    hexDigits[1] = result[i + 3];

                unsigned charValue = 0;
                sscanf(hexDigits, "%x", &charValue);

                replacement[0] = (char)charValue;
                charsToRemove = 2 + qstrlen(hexDigits);
            }
            break;
            default:
                escapedChar = false;
            }

            if (escapedChar)
                result.replace(i, charsToRemove, replacement);
        }
    }

    return result;
}

void KeyboardTranslator::Entry::insertModifier(QString& item , int modifier) const
{
    if (!(modifier & _modifierMask))
        return;

    if (modifier & _modifiers)
        item += '+';
    else
        item += '-';

    if (modifier == Qt::ShiftModifier)
        item += "Shift";
    else if (modifier == Qt::ControlModifier)
        item += "Ctrl";
    else if (modifier == Qt::AltModifier)
        item += "Alt";
    else if (modifier == Qt::MetaModifier)
        item += "Meta";
    else if (modifier == Qt::KeypadModifier)
        item += "KeyPad";
}
void KeyboardTranslator::Entry::insertState(QString& item, int aState) const
{
    if (!(aState & _stateMask))
        return;

    if (aState & _state)
        item += '+';
    else
        item += '-';

    if (aState == KeyboardTranslator::AlternateScreenState)
        item += "AppScreen";
    else if (aState == KeyboardTranslator::NewLineState)
        item += "NewLine";
    else if (aState == KeyboardTranslator::AnsiState)
        item += "Ansi";
    else if (aState == KeyboardTranslator::CursorKeysState)
        item += "AppCursorKeys";
    else if (aState == KeyboardTranslator::AnyModifierState)
        item += "AnyModifier";
    else if (aState == KeyboardTranslator::ApplicationKeypadState)
        item += "AppKeypad";
}
QString KeyboardTranslator::Entry::resultToString(bool expandWildCards,
        Qt::KeyboardModifiers keyboardModifiers) const
{
    if (!_text.isEmpty())
        return escapedText(expandWildCards, keyboardModifiers);
    else if (_command == EraseCommand)
        return "Erase";
    else if (_command == ScrollPageUpCommand)
        return "ScrollPageUp";
    else if (_command == ScrollPageDownCommand)
        return "ScrollPageDown";
    else if (_command == ScrollLineUpCommand)
        return "ScrollLineUp";
    else if (_command == ScrollLineDownCommand)
        return "ScrollLineDown";
    else if (_command == ScrollUpToTopCommand)
        return "ScrollUpToTop";
    else if (_command == ScrollDownToBottomCommand)
        return "ScrollDownToBottom";

    return QString();
}
QString KeyboardTranslator::Entry::conditionToString() const
{
    QString result = QKeySequence(_keyCode).toString();

    insertModifier(result , Qt::ShiftModifier);
    insertModifier(result , Qt::ControlModifier);
    insertModifier(result , Qt::AltModifier);
    insertModifier(result , Qt::MetaModifier);
    insertModifier(result , Qt::KeypadModifier);

    insertState(result , KeyboardTranslator::AlternateScreenState);
    insertState(result , KeyboardTranslator::NewLineState);
    insertState(result , KeyboardTranslator::AnsiState);
    insertState(result , KeyboardTranslator::CursorKeysState);
    insertState(result , KeyboardTranslator::AnyModifierState);
    insertState(result , KeyboardTranslator::ApplicationKeypadState);

    return result;
}

KeyboardTranslator::KeyboardTranslator(const QString& aName)
    : _name(aName)
{
}

FallbackKeyboardTranslator::FallbackKeyboardTranslator()
    : KeyboardTranslator("fallback")
{
    setDescription("Fallback Keyboard Translator");

    // Key "TAB" should send out '\t'
    KeyboardTranslator::Entry entry;
    entry.setKeyCode(Qt::Key_Tab);
    entry.setText("\t");
    addEntry(entry);
}

void KeyboardTranslator::setDescription(const QString& aDescription)
{
    _description = aDescription;
}

QString KeyboardTranslator::description() const
{
    return _description;
}

void KeyboardTranslator::setName(const QString& aName)
{
    _name = aName;
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
    _entries.insert(keyCode, entry);
}

void KeyboardTranslator::replaceEntry(const Entry& existing , const Entry& replacement)
{
    if (!existing.isNull())
        _entries.remove(existing.keyCode(), existing);

    _entries.insert(replacement.keyCode(), replacement);
}

void KeyboardTranslator::removeEntry(const Entry& entry)
{
    _entries.remove(entry.keyCode(), entry);
}

KeyboardTranslator::Entry KeyboardTranslator::findEntry(int keyCode, Qt::KeyboardModifiers modifiers, States state) const
{
    foreach(const Entry & entry, _entries.values(keyCode)) {
        if (entry.matches(keyCode, modifiers, state))
            return entry;
    }

    return Entry(); // No matching entry
}
