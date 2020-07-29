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

#include "../konsoledebug.h"

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
    if ((modifier & _modifierMask) == 0U) {
        return;
    }

    if ((modifier & _modifiers) != 0U) {
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
