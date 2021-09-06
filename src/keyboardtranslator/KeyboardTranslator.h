/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KEYBOARDTRANSLATOR_H
#define KEYBOARDTRANSLATOR_H

// Qt
#include <QList>
#include <QMetaType>
#include <QMultiHash>
#include <QString>

// Konsole
#include "../konsoleprivate_export.h"

class QIODevice;
class QTextStream;

namespace Konsole
{
/**
 * A converter which maps between key sequences pressed by the user and the
 * character strings which should be sent to the terminal and commands
 * which should be invoked when those character sequences are pressed.
 *
 * Konsole supports multiple keyboard translators, allowing the user to
 * specify the character sequences which are sent to the terminal
 * when particular key sequences are pressed.
 *
 * A key sequence is defined as a key code, associated keyboard modifiers
 * (Shift,Ctrl,Alt,Meta etc.) and state flags which indicate the state
 * which the terminal must be in for the key sequence to apply.
 */
class KONSOLEPRIVATE_EXPORT KeyboardTranslator
{
public:
    /**
     * The meaning of a particular key sequence may depend upon the state which
     * the terminal emulation is in.  Therefore findEntry() may return a different
     * Entry depending upon the state flags supplied.
     *
     * This enum describes the states which may be associated with a particular
     * entry in the keyboard translation entry.
     */
    enum State {
        /** Indicates that no special state is active */
        NoState = 0,
        /**
         * TODO More documentation
         */
        NewLineState = 1,
        /**
         * Indicates that the terminal is in 'ANSI' mode.
         * TODO: More documentation
         */
        AnsiState = 2,
        /**
         * TODO More documentation
         */
        CursorKeysState = 4,
        /**
         * Indicates that the alternate screen ( typically used by interactive programs
         * such as screen or vim ) is active
         */
        AlternateScreenState = 8,
        /** Indicates that any of the modifier keys is active. */
        AnyModifierState = 16,
        /** Indicates that the numpad is in application mode. */
        ApplicationKeypadState = 32,
    };
    Q_DECLARE_FLAGS(States, State)

    /**
     * This enum describes commands which are associated with particular key sequences.
     */
    enum Command {
        /** Indicates that no command is associated with this command sequence */
        NoCommand = 0,
        /** TODO Document me */
        SendCommand = 1,
        /** Scroll the terminal display up one page */
        ScrollPageUpCommand = 2,
        /** Scroll the terminal display down one page */
        ScrollPageDownCommand = 4,
        /** Scroll the terminal display up one line */
        ScrollLineUpCommand = 8,
        /** Scroll the terminal display down one line */
        ScrollLineDownCommand = 16,
        /** Scroll the terminal display up to the start of history */
        ScrollUpToTopCommand = 32,
        /** Scroll the terminal display down to the end of history */
        ScrollDownToBottomCommand = 64,
        /** Echos the operating system specific erase character. */
        EraseCommand = 256,
    };
    Q_DECLARE_FLAGS(Commands, Command)

    /**
     * Represents an association between a key sequence pressed by the user
     * and the character sequence and commands associated with it for a particular
     * KeyboardTranslator.
     */
    class Entry
    {
    public:
        /**
         * Constructs a new entry for a keyboard translator.
         */
        Entry();

        /**
         * Returns true if this entry is null.
         * This is true for newly constructed entries which have no properties set.
         */
        bool isNull() const;

        /** Returns the commands associated with this entry */
        Command command() const;
        /** Sets the command associated with this entry. */
        void setCommand(Command aCommand);

        /**
         * Returns the character sequence associated with this entry, optionally replacing
         * wildcard '*' characters with numbers to indicate the keyboard modifiers being pressed.
         *
         * TODO: The numbers used to replace '*' characters are taken from the Konsole/KDE 3 code.
         * Document them.
         *
         * @param expandWildCards Specifies whether wild cards (occurrences of the '*' character) in
         * the entry should be replaced with a number to indicate the modifier keys being pressed.
         *
         * @param keyboardModifiers The keyboard modifiers being pressed.
         */
        QByteArray text(bool expandWildCards = false, Qt::KeyboardModifiers keyboardModifiers = Qt::NoModifier) const;

        /** Sets the character sequence associated with this entry */
        void setText(const QByteArray &aText);

        /**
         * Returns the character sequence associated with this entry,
         * with any non-printable characters replaced with escape sequences.
         *
         * eg. \\E for Escape, \\t for tab, \\n for new line.
         *
         * @param expandWildCards See text()
         * @param keyboardModifiers The keyboard modifiers being pressed.
         */
        QByteArray escapedText(bool expandWildCards = false, Qt::KeyboardModifiers keyboardModifiers = Qt::NoModifier) const;

        /** Returns the character code ( from the Qt::Key enum ) associated with this entry */
        int keyCode() const;
        /** Sets the character code associated with this entry */
        void setKeyCode(int aKeyCode);

        /**
         * Returns a bitwise-OR of the enabled keyboard modifiers associated with this entry.
         * If a modifier is set in modifierMask() but not in modifiers(), this means that the entry
         * only matches when that modifier is NOT pressed.
         *
         * If a modifier is not set in modifierMask() then the entry matches whether the modifier
         * is pressed or not.
         */
        Qt::KeyboardModifiers modifiers() const;

        /** Returns the keyboard modifiers which are valid in this entry.  See modifiers() */
        Qt::KeyboardModifiers modifierMask() const;

        /** See modifiers() */
        void setModifiers(Qt::KeyboardModifiers modifiers);
        /** See modifierMask() and modifiers() */
        void setModifierMask(Qt::KeyboardModifiers mask);

        /**
         * Returns a bitwise-OR of the enabled state flags associated with this entry.
         * If flag is set in stateMask() but not in state(), this means that the entry only
         * matches when the terminal is NOT in that state.
         *
         * If a state is not set in stateMask() then the entry matches whether the terminal
         * is in that state or not.
         */
        States state() const;

        /** Returns the state flags which are valid in this entry.  See state() */
        States stateMask() const;

        /** See state() */
        void setState(States aState);
        /** See stateMask() */
        void setStateMask(States aStateMask);

        /**
         * Returns this entry's conditions ( ie. its key code, modifier and state criteria )
         * as a string.
         */
        QString conditionToString() const;

        /**
         * Returns this entry's result ( ie. its command or character sequence )
         * as a string.
         *
         * @param expandWildCards See text()
         * @param keyboardModifiers The keyboard modifiers being pressed.
         */
        QString resultToString(bool expandWildCards = false, Qt::KeyboardModifiers keyboardModifiers = Qt::NoModifier) const;

        /**
         * Returns true if this entry matches the given key sequence, specified
         * as a combination of @p testKeyCode , @p testKeyboardModifiers and @p testState.
         */
        bool matches(int testKeyCode, Qt::KeyboardModifiers testKeyboardModifiers, States testState) const;

        bool operator==(const Entry &rhs) const;

    private:
        void insertModifier(QString &item, int modifier) const;
        void insertState(QString &item, int state) const;
        QByteArray unescape(const QByteArray &text) const;

        int _keyCode;
        Qt::KeyboardModifiers _modifiers;
        Qt::KeyboardModifiers _modifierMask;
        States _state;
        States _stateMask;

        Command _command;
        QByteArray _text;
    };

    /** Constructs a new keyboard translator with the given @p name */
    explicit KeyboardTranslator(const QString &name);

    /** Returns the name of this keyboard translator */
    QString name() const;

    /** Sets the name of this keyboard translator */
    void setName(const QString &name);

    /** Returns the descriptive name of this keyboard translator */
    QString description() const;

    /** Sets the descriptive name of this keyboard translator */
    void setDescription(const QString &description);

    /**
     * Looks for an entry in this keyboard translator which matches the given
     * key code, keyboard modifiers and state flags.
     *
     * Returns the matching entry if found or a null Entry otherwise ( ie.
     * entry.isNull() will return true )
     *
     * @param keyCode A key code from the Qt::Key enum
     * @param modifiers A combination of modifiers
     * @param state Optional flags which specify the current state of the terminal
     */
    Entry findEntry(int keyCode, Qt::KeyboardModifiers modifiers, States state = NoState) const;

    /**
     * Adds an entry to this keyboard translator's table.  Entries can be looked up according
     * to their key sequence using findEntry()
     */
    void addEntry(const Entry &entry);

    /**
     * Replaces an entry in the translator.  If the @p existing entry is null,
     * then this is equivalent to calling addEntry(@p replacement)
     */
    void replaceEntry(const Entry &existing, const Entry &replacement);

    /**
     * Removes an entry from the table.
     */
    void removeEntry(const Entry &entry);

    /** Returns a list of all entries in the translator. */
    QList<Entry> entries() const;

private:
    // All entries in this translator, indexed by their keycode
    QMultiHash<int, Entry> _entries;

    QString _name;
    QString _description;
};
Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardTranslator::States)
Q_DECLARE_OPERATORS_FOR_FLAGS(KeyboardTranslator::Commands)

inline int KeyboardTranslator::Entry::keyCode() const
{
    return _keyCode;
}

inline void KeyboardTranslator::Entry::setKeyCode(int aKeyCode)
{
    _keyCode = aKeyCode;
}

inline void KeyboardTranslator::Entry::setModifiers(Qt::KeyboardModifiers modifiers)
{
    _modifiers = modifiers;
}

inline Qt::KeyboardModifiers KeyboardTranslator::Entry::modifiers() const
{
    return _modifiers;
}

inline void KeyboardTranslator::Entry::setModifierMask(Qt::KeyboardModifiers mask)
{
    _modifierMask = mask;
}

inline Qt::KeyboardModifiers KeyboardTranslator::Entry::modifierMask() const
{
    return _modifierMask;
}

inline bool KeyboardTranslator::Entry::isNull() const
{
    return *this == Entry();
}

inline void KeyboardTranslator::Entry::setCommand(Command aCommand)
{
    _command = aCommand;
}

inline KeyboardTranslator::Command KeyboardTranslator::Entry::command() const
{
    return _command;
}

inline void KeyboardTranslator::Entry::setText(const QByteArray &aText)
{
    _text = unescape(aText);
}

inline int oneOrZero(int value)
{
    return value ? 1 : 0;
}

inline QByteArray KeyboardTranslator::Entry::text(bool expandWildCards, Qt::KeyboardModifiers keyboardModifiers) const
{
    QByteArray expandedText = _text;

    if (expandWildCards) {
        int modifierValue = 1;
        modifierValue += oneOrZero(keyboardModifiers & Qt::ShiftModifier);
        modifierValue += oneOrZero(keyboardModifiers & Qt::AltModifier) << 1;
        modifierValue += oneOrZero(keyboardModifiers & Qt::ControlModifier) << 2;

        for (int i = 0; i < _text.length(); i++) {
            if (expandedText[i] == '*') {
                expandedText[i] = '0' + modifierValue;
            }
        }
    }

    return expandedText;
}

inline void KeyboardTranslator::Entry::setState(States aState)
{
    _state = aState;
}

inline KeyboardTranslator::States KeyboardTranslator::Entry::state() const
{
    return _state;
}

inline void KeyboardTranslator::Entry::setStateMask(States aStateMask)
{
    _stateMask = aStateMask;
}

inline KeyboardTranslator::States KeyboardTranslator::Entry::stateMask() const
{
    return _stateMask;
}
}

Q_DECLARE_METATYPE(Konsole::KeyboardTranslator::Entry)
Q_DECLARE_METATYPE(const Konsole::KeyboardTranslator *)

#endif // KEYBOARDTRANSLATOR_H
