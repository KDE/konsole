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

#include "konsoledebug.h"

// System
#include <cctype>
#include <cstdio>

// Qt
#include <QBuffer>
#include <QTextStream>
#include <QRegularExpression>
#include <QKeySequence>

// KDE
#include <KLocalizedString>

using namespace Konsole;

KeyboardTranslatorWriter::KeyboardTranslatorWriter(QIODevice *destination) :
    _destination(destination),
    _writer(nullptr)
{
    Q_ASSERT(destination && destination->isWritable());

    _writer = new QTextStream(_destination);
}

KeyboardTranslatorWriter::~KeyboardTranslatorWriter()
{
    delete _writer;
}

void KeyboardTranslatorWriter::writeHeader(const QString &description)
{
    *_writer << "keyboard \"" << description << '\"' << '\n';
}

void KeyboardTranslatorWriter::writeEntry(const KeyboardTranslator::Entry &entry)
{
    QString result;
    if (entry.command() != KeyboardTranslator::NoCommand) {
        result = entry.resultToString();
    } else {
        result = QLatin1Char('\"') + entry.resultToString() + QLatin1Char('\"');
    }

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

KeyboardTranslatorReader::KeyboardTranslatorReader(QIODevice *source) :
    _source(source),
    _description(QString()),
    _nextEntry(),
    _hasNext(false)
{
    // read input until we find the description
    while (_description.isEmpty() && !source->atEnd()) {
        QList<Token> tokens = tokenize(QString::fromLocal8Bit(source->readLine()));
        if (!tokens.isEmpty() && tokens.first().type == Token::TitleKeyword) {
            _description = i18n(tokens[1].text.toUtf8().constData());
        }
    }
    // read first entry (if any)
    readNext();
}

KeyboardTranslatorReader::~KeyboardTranslatorReader() = default;

void KeyboardTranslatorReader::readNext()
{
    // find next entry
    while (!_source->atEnd()) {
        const QList<Token> &tokens = tokenize(QString::fromLocal8Bit(_source->readLine()));
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
                if (!parseAsCommand(tokens[2].text, command)) {
                    qCDebug(KonsoleDebug) << "Key" << tokens[1].text << ", Command"
                                          << tokens[2].text << "not understood. ";
                }
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

bool KeyboardTranslatorReader::parseAsCommand(const QString &text,
                                              KeyboardTranslator::Command &command)
{
    if (text.compare(QLatin1String("erase"), Qt::CaseInsensitive) == 0) {
        command = KeyboardTranslator::EraseCommand;
    } else if (text.compare(QLatin1String("scrollpageup"), Qt::CaseInsensitive) == 0) {
        command = KeyboardTranslator::ScrollPageUpCommand;
    } else if (text.compare(QLatin1String("scrollpagedown"), Qt::CaseInsensitive) == 0) {
        command = KeyboardTranslator::ScrollPageDownCommand;
    } else if (text.compare(QLatin1String("scrolllineup"), Qt::CaseInsensitive) == 0) {
        command = KeyboardTranslator::ScrollLineUpCommand;
    } else if (text.compare(QLatin1String("scrolllinedown"), Qt::CaseInsensitive) == 0) {
        command = KeyboardTranslator::ScrollLineDownCommand;
    } else if (text.compare(QLatin1String("scrolluptotop"), Qt::CaseInsensitive) == 0) {
        command = KeyboardTranslator::ScrollUpToTopCommand;
    } else if (text.compare(QLatin1String("scrolldowntobottom"), Qt::CaseInsensitive) == 0) {
        command = KeyboardTranslator::ScrollDownToBottomCommand;
    } else {
        return false;
    }

    return true;
}

bool KeyboardTranslatorReader::decodeSequence(const QString &text, int &keyCode,
                                              Qt::KeyboardModifiers &modifiers,
                                              Qt::KeyboardModifiers &modifierMask,
                                              KeyboardTranslator::States &flags,
                                              KeyboardTranslator::States &flagMask)
{
    bool isWanted = true;
    QString buffer;

    Qt::KeyboardModifiers tempModifiers = modifiers;
    Qt::KeyboardModifiers tempModifierMask = modifierMask;
    KeyboardTranslator::States tempFlags = flags;
    KeyboardTranslator::States tempFlagMask = flagMask;

    for (int i = 0; i < text.count(); i++) {
        const QChar &ch = text[i];
        const bool isFirstLetter = (i == 0);
        const bool isLastLetter = (i == text.count() - 1);
        bool endOfItem = true;
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

                if (isWanted) {
                    tempModifiers |= itemModifier;
                }
            } else if (parseAsStateFlag(buffer, itemFlag)) {
                tempFlagMask |= itemFlag;

                if (isWanted) {
                    tempFlags |= itemFlag;
                }
            } else if (parseAsKeyCode(buffer, itemKeyCode)) {
                keyCode = itemKeyCode;
            } else {
                qCDebug(KonsoleDebug) << "Unable to parse key binding item:" << buffer;
            }

            buffer.clear();
        }

        // check if this is a wanted / not-wanted flag and update the
        // state ready for the next item
        if (ch == QLatin1Char('+')) {
            isWanted = true;
        } else if (ch == QLatin1Char('-')) {
            isWanted = false;
        }
    }

    modifiers = tempModifiers;
    modifierMask = tempModifierMask;
    flags = tempFlags;
    flagMask = tempFlagMask;

    return true;
}

bool KeyboardTranslatorReader::parseAsModifier(const QString &item, Qt::KeyboardModifier &modifier)
{
    if (item == QLatin1String("shift")) {
        modifier = Qt::ShiftModifier;
    } else if (item == QLatin1String("ctrl") || item == QLatin1String("control")) {
        modifier = Qt::ControlModifier;
    } else if (item == QLatin1String("alt")) {
        modifier = Qt::AltModifier;
    } else if (item == QLatin1String("meta")) {
        modifier = Qt::MetaModifier;
    } else if (item == QLatin1String("keypad")) {
        modifier = Qt::KeypadModifier;
    } else {
        return false;
    }

    return true;
}

bool KeyboardTranslatorReader::parseAsStateFlag(const QString &item,
                                                KeyboardTranslator::State &flag)
{
    if (item == QLatin1String("appcukeys") || item == QLatin1String("appcursorkeys")) {
        flag = KeyboardTranslator::CursorKeysState;
    } else if (item == QLatin1String("ansi")) {
        flag = KeyboardTranslator::AnsiState;
    } else if (item == QLatin1String("newline")) {
        flag = KeyboardTranslator::NewLineState;
    } else if (item == QLatin1String("appscreen")) {
        flag = KeyboardTranslator::AlternateScreenState;
    } else if (item == QLatin1String("anymod") || item == QLatin1String("anymodifier")) {
        flag = KeyboardTranslator::AnyModifierState;
    } else if (item == QLatin1String("appkeypad")) {
        flag = KeyboardTranslator::ApplicationKeypadState;
    } else {
        return false;
    }

    return true;
}

bool KeyboardTranslatorReader::parseAsKeyCode(const QString &item, int &keyCode)
{
    QKeySequence sequence = QKeySequence::fromString(item);
    if (!sequence.isEmpty()) {
        keyCode = sequence[0];

        if (sequence.count() > 1) {
            qCDebug(KonsoleDebug) << "Unhandled key codes in sequence: " << item;
        }
    } else {
        return false;
    }

    return true;
}

QString KeyboardTranslatorReader::description() const
{
    return _description;
}

bool KeyboardTranslatorReader::hasNextEntry() const
{
    return _hasNext;
}

KeyboardTranslator::Entry KeyboardTranslatorReader::createEntry(const QString &condition,
                                                                const QString &result)
{
    QString entryString(QStringLiteral("keyboard \"temporary\"\nkey "));
    entryString.append(condition);
    entryString.append(QLatin1String(" : "));

    // if 'result' is the name of a command then the entry result will be that command,
    // otherwise the result will be treated as a string to echo when the key sequence
    // specified by 'condition' is pressed
    KeyboardTranslator::Command command;
    if (parseAsCommand(result, command)) {
        entryString.append(result);
    } else {
        entryString.append(QLatin1Char('\"') + result + QLatin1Char('\"'));
    }

    QByteArray array = entryString.toUtf8();
    QBuffer buffer(&array);
    buffer.open(QIODevice::ReadOnly);
    KeyboardTranslatorReader reader(&buffer);

    KeyboardTranslator::Entry entry;
    if (reader.hasNextEntry()) {
        entry = reader.nextEntry();
    }

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

QList<KeyboardTranslatorReader::Token> KeyboardTranslatorReader::tokenize(const QString &line)
{
    QString text = line;

    // remove comments
    bool inQuotes = false;
    int commentPos = -1;
    for (int i = text.length() - 1; i >= 0; i--) {
        QChar ch = text[i];
        if (ch == QLatin1Char('\"')) {
            inQuotes = !inQuotes;
        } else if (ch == QLatin1Char('#') && !inQuotes) {
            commentPos = i;
        }
    }
    if (commentPos != -1) {
        text.remove(commentPos, text.length());
    }

    text = text.simplified();

    // title line: keyboard "title"
    static const QRegularExpression title(QStringLiteral("keyboard\\s+\"(.*)\""),
                                          QRegularExpression::OptimizeOnFirstUsageOption);
    // key line: key KeySequence : "output"
    // key line: key KeySequence : command
    static const QRegularExpression key(QStringLiteral("key\\s+(.+?)\\s*:\\s*(\"(.*)\"|\\w+)"),
                                        QRegularExpression::OptimizeOnFirstUsageOption);

    QList<Token> list;
    if (text.isEmpty()) {
        return list;
    }

    QRegularExpressionMatch titleMatch(title.match(text));

    if (titleMatch.hasMatch()) {
        Token titleToken = { Token::TitleKeyword, QString() };
        Token textToken = { Token::TitleText, titleMatch.captured(1) };

        list << titleToken << textToken;
        return list;
    }

    QRegularExpressionMatch keyMatch(key.match(text));
    if (!keyMatch.hasMatch()) {
        qCDebug(KonsoleDebug) << "Line in keyboard translator file could not be understood:"
                              << text;
        return list;
    }

    Token keyToken = { Token::KeyKeyword, QString() };
    QString sequenceTokenString = keyMatch.captured(1);
    Token sequenceToken = { Token::KeySequence, sequenceTokenString.remove(QLatin1Char(' ')) };

    list << keyToken << sequenceToken;

    if (keyMatch.capturedRef(3).isEmpty()) {
        // capturedTexts().at(2) is a command
        Token commandToken = { Token::Command, keyMatch.captured(2) };
        list << commandToken;
    } else {
        // capturedTexts().at(3) is the output string
        Token outputToken = { Token::OutputText, keyMatch.captured(3) };
        list << outputToken;
    }

    return list;
}

KeyboardTranslator::Entry::Entry() :
    _keyCode(0),
    _modifiers(Qt::NoModifier),
    _modifierMask(Qt::NoModifier),
    _state(NoState),
    _stateMask(NoState),
    _command(NoCommand),
    _text(QByteArray())
{
}

bool KeyboardTranslator::Entry::operator==(const Entry &rhs) const
{
    return _keyCode == rhs._keyCode
           && _modifiers == rhs._modifiers
           && _modifierMask == rhs._modifierMask
           && _state == rhs._state
           && _stateMask == rhs._stateMask
           && _command == rhs._command
           && _text == rhs._text;
}

bool KeyboardTranslator::Entry::matches(int testKeyCode,
                                        Qt::KeyboardModifiers testKeyboardModifiers,
                                        States testState) const
{
    if (_keyCode != testKeyCode) {
        return false;
    }

    if ((testKeyboardModifiers & _modifierMask) != (_modifiers & _modifierMask)) {
        return false;
    }

    // if testKeyboardModifiers is non-zero, the 'any modifier' state is implicit
    if (testKeyboardModifiers != 0) {
        testState |= AnyModifierState;
    }

    if ((testState & _stateMask) != (_state & _stateMask)) {
        return false;
    }

    // special handling for the 'Any Modifier' state, which checks for the presence of
    // any or no modifiers.  In this context, the 'keypad' modifier does not count.
    bool anyModifiersSet = (testKeyboardModifiers != 0)
                           && (testKeyboardModifiers != Qt::KeypadModifier);
    bool wantAnyModifier = (_state &KeyboardTranslator::AnyModifierState) != 0;
    if ((_stateMask &KeyboardTranslator::AnyModifierState) != 0) {
        if (wantAnyModifier != anyModifiersSet) {
            return false;
        }
    }

    return true;
}

QByteArray KeyboardTranslator::Entry::escapedText(bool expandWildCards,
                                                  Qt::KeyboardModifiers keyboardModifiers) const
{
    QByteArray result(text(expandWildCards, keyboardModifiers));

    for (int i = 0; i < result.count(); i++) {
        const char ch = result[i];
        char replacement = 0;

        switch (ch) {
        case 27:
            replacement = 'E';
            break;
        case 8:
            replacement = 'b';
            break;
        case 12:
            replacement = 'f';
            break;
        case 9:
            replacement = 't';
            break;
        case 13:
            replacement = 'r';
            break;
        case 10:
            replacement = 'n';
            break;
        default:
            // any character which is not printable is replaced by an equivalent
            // \xhh escape sequence (where 'hh' are the corresponding hex digits)
            if (!QChar(QLatin1Char(ch)).isPrint()) {
                replacement = 'x';
            }
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

QByteArray KeyboardTranslator::Entry::unescape(const QByteArray &text) const
{
    QByteArray result(text);

    for (int i = 0; i < result.count() - 1; i++) {
        QByteRef ch = result[i];
        if (ch == '\\') {
            char replacement[2] = {0, 0};
            int charsToRemove = 2;
            bool escapedChar = true;

            switch (result[i + 1]) {
            case 'E':
                replacement[0] = 27;
                break;
            case 'b':
                replacement[0] = 8;
                break;
            case 'f':
                replacement[0] = 12;
                break;
            case 't':
                replacement[0] = 9;
                break;
            case 'r':
                replacement[0] = 13;
                break;
            case 'n':
                replacement[0] = 10;
                break;
            case 'x':
            {
                // format is \xh or \xhh where 'h' is a hexadecimal
                // digit from 0-9 or A-F which should be replaced
                // with the corresponding character value
                char hexDigits[3] = {0};

                if ((i < result.count() - 2) && (isxdigit(result[i + 2]) != 0)) {
                    hexDigits[0] = result[i + 2];
                }
                if ((i < result.count() - 3) && (isxdigit(result[i + 3]) != 0)) {
                    hexDigits[1] = result[i + 3];
                }

                unsigned charValue = 0;
                sscanf(hexDigits, "%2x", &charValue);

                replacement[0] = static_cast<char>(charValue);
                charsToRemove = 2 + qstrlen(hexDigits);
                break;
            }
            default:
                escapedChar = false;
            }

            if (escapedChar) {
                result.replace(i, charsToRemove, replacement, 1);
            }
        }
    }

    return result;
}

void KeyboardTranslator::Entry::insertModifier(QString &item, int modifier) const
{
    if ((modifier & _modifierMask) == 0u) {
        return;
    }

    if ((modifier & _modifiers) != 0u) {
        item += QLatin1Char('+');
    } else {
        item += QLatin1Char('-');
    }

    if (modifier == Qt::ShiftModifier) {
        item += QLatin1String("Shift");
    } else if (modifier == Qt::ControlModifier) {
        item += QLatin1String("Ctrl");
    } else if (modifier == Qt::AltModifier) {
        item += QLatin1String("Alt");
    } else if (modifier == Qt::MetaModifier) {
        item += QLatin1String("Meta");
    } else if (modifier == Qt::KeypadModifier) {
        item += QLatin1String("KeyPad");
    }
}

void KeyboardTranslator::Entry::insertState(QString &item, int state) const
{
    if ((state & _stateMask) == 0) {
        return;
    }

    if ((state & _state) != 0) {
        item += QLatin1Char('+');
    } else {
        item += QLatin1Char('-');
    }

    if (state == KeyboardTranslator::AlternateScreenState) {
        item += QLatin1String("AppScreen");
    } else if (state == KeyboardTranslator::NewLineState) {
        item += QLatin1String("NewLine");
    } else if (state == KeyboardTranslator::AnsiState) {
        item += QLatin1String("Ansi");
    } else if (state == KeyboardTranslator::CursorKeysState) {
        item += QLatin1String("AppCursorKeys");
    } else if (state == KeyboardTranslator::AnyModifierState) {
        item += QLatin1String("AnyModifier");
    } else if (state == KeyboardTranslator::ApplicationKeypadState) {
        item += QLatin1String("AppKeypad");
    }
}

QString KeyboardTranslator::Entry::resultToString(bool expandWildCards,
                                                  Qt::KeyboardModifiers keyboardModifiers) const
{
    if (!_text.isEmpty()) {
        return QString::fromLatin1(escapedText(expandWildCards, keyboardModifiers));
    } else if (_command == EraseCommand) {
        return QStringLiteral("Erase");
    } else if (_command == ScrollPageUpCommand) {
        return QStringLiteral("ScrollPageUp");
    } else if (_command == ScrollPageDownCommand) {
        return QStringLiteral("ScrollPageDown");
    } else if (_command == ScrollLineUpCommand) {
        return QStringLiteral("ScrollLineUp");
    } else if (_command == ScrollLineDownCommand) {
        return QStringLiteral("ScrollLineDown");
    } else if (_command == ScrollUpToTopCommand) {
        return QStringLiteral("ScrollUpToTop");
    } else if (_command == ScrollDownToBottomCommand) {
        return QStringLiteral("ScrollDownToBottom");
    }

    return QString();
}

QString KeyboardTranslator::Entry::conditionToString() const
{
    QString result = QKeySequence(_keyCode).toString();

    insertModifier(result, Qt::ShiftModifier);
    insertModifier(result, Qt::ControlModifier);
    insertModifier(result, Qt::AltModifier);
    insertModifier(result, Qt::MetaModifier);
    insertModifier(result, Qt::KeypadModifier);

    insertState(result, KeyboardTranslator::AlternateScreenState);
    insertState(result, KeyboardTranslator::NewLineState);
    insertState(result, KeyboardTranslator::AnsiState);
    insertState(result, KeyboardTranslator::CursorKeysState);
    insertState(result, KeyboardTranslator::AnyModifierState);
    insertState(result, KeyboardTranslator::ApplicationKeypadState);

    return result;
}

KeyboardTranslator::KeyboardTranslator(const QString &name) :
    _entries(QMultiHash<int, Entry>()),
    _name(name),
    _description(QString())
{
}

FallbackKeyboardTranslator::FallbackKeyboardTranslator() :
    KeyboardTranslator(QStringLiteral("fallback"))
{
    setDescription(QStringLiteral("Fallback Keyboard Translator"));

    // Key "TAB" should send out '\t'
    KeyboardTranslator::Entry entry;
    entry.setKeyCode(Qt::Key_Tab);
    entry.setText("\t");
    addEntry(entry);
}

void KeyboardTranslator::setDescription(const QString &description)
{
    _description = description;
}

QString KeyboardTranslator::description() const
{
    return _description;
}

void KeyboardTranslator::setName(const QString &name)
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

void KeyboardTranslator::addEntry(const Entry &entry)
{
    const int keyCode = entry.keyCode();
    _entries.insert(keyCode, entry);
}

void KeyboardTranslator::replaceEntry(const Entry &existing, const Entry &replacement)
{
    if (!existing.isNull()) {
        _entries.remove(existing.keyCode(), existing);
    }

    _entries.insert(replacement.keyCode(), replacement);
}

void KeyboardTranslator::removeEntry(const Entry &entry)
{
    _entries.remove(entry.keyCode(), entry);
}

KeyboardTranslator::Entry KeyboardTranslator::findEntry(int keyCode,
                                                        Qt::KeyboardModifiers modifiers,
                                                        States state) const
{
    QHash<int, KeyboardTranslator::Entry>::const_iterator i = _entries.find(keyCode);
    while (i != _entries.constEnd() && i.key() == keyCode) {
        if (i.value().matches(keyCode, modifiers, state)) {
            return i.value();
        }
        ++i;
    }

    return Entry(); // No matching entry
}
