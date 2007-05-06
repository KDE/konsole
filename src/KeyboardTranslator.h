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

#ifndef KEYBOARDTRANSLATOR_H
#define KEYBOARDTRANSLATOR_H

// Qt
#include <QMultiHash>
#include <QList>
#include <QKeySequence>
#include <QMetaType>
#include <QVarLengthArray>

class QIODevice;

namespace Konsole
{

/** 
 * A convertor which maps between key sequences pressed by the user and the
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
class KeyboardTranslator
{
public:
    /** 
     * The meaning of a particular key sequence may depend upon the state which
     * the terminal emulation is in.  Therefore findEntry() may return a different
     * Entry depending upon the state flags supplied.
     *
     * This enum describes the states which may be associated with with a particular
     * entry in the keyboard translation entry.
     */
    enum State
    {
        /** Indicates that no special state is active */
        NoState = 0,
        /**
         * TODO More documentation
         */
        NewLineState = 1,
        /** 
         * Indicates that the terminal is in 'Ansi' mode.
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
        AlternateScreenState = 8
    };

    /**
     * This enum describes commands which are associated with particular key sequences.
     */
    enum Command
    {
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
        /** Toggles scroll lock mode */
        ScrollLockCommand = 32
    };

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
         *
         * @param keyCode A key code from the Qt::Key enum
         * @param modifiers The combination of keyboard modifiers
         * @param flags The state flags associated with this entry
         * @param text The character sequence which should be sent to the terminal when
         * the key sequence described by the @p keyCode, @p modifiers and @p state is activated.
         * @param command The commands which should be executed when the key sequence 
         * described by the @p keyCode, @p modifiers and @p state is activated.
         */
        Entry( int keyCode , 
               Qt::KeyboardModifier modifiers , 
               State flags,
               const QByteArray& text,
               Command command );
        Entry();

        /** Returns the commands associated with this entry */
        Command command() const;
        /** Returns the character sequence associated with this entry */
        QByteArray text() const;

        /** Returns the character code ( from the Qt::Key enum ) associated with this entry */
        int keyCode() const;
        /** Returns the keyboard modifiers associated with this entry */
        Qt::KeyboardModifier modifiers() const;
        /** Returns the state flags associated with this entry */
        State state() const;

        /** 
         * Returns the key code and modifiers associated with this entry 
         * as a QKeySequence
         */
        QKeySequence keySequence() const;

        /** 
         * Returns true if this entry matches the given key sequence, specified
         * as a combination of @p keyCode , @p modifiers and @p state.
         */
        bool matches( int keyCode , Qt::KeyboardModifier modifiers , State flags ) const;

       
    private:
        int _keyCode;
        Qt::KeyboardModifier _modifiers;
        State _state;

        Command _command;
        QByteArray _text;
    };

    /** Constructs a new keyboard translator with the given @p name */
    KeyboardTranslator(const QString& name);
   
    KeyboardTranslator(const KeyboardTranslator& other);

    /** Returns the name of this keyboard translator */
    QString name() const;

    /** Sets the name of this keyboard translator */
    void setName(const QString& name);

    /** Returns the descriptive name of this keyboard translator */
    QString description() const;

    /** Sets the descriptive name of this keyboard translator */
    void setDescription(const QString& description);

    /**
     * Looks for an entry in this keyboard translator which matches the given
     * key code, keyboard modifiers and state flags.
     * 
     * Returns the matching entry if found or 0 if there is no matching entry
     * in this keyboard translation.
     *
     * @param keyCode A key code from the Qt::Key enum
     * @param modifiers A combination of modifiers
     * @param state Optional flags which specify the current state of the terminal
     */
    Entry findEntry(int keyCode , Qt::KeyboardModifier modifiers , State state = NoState) const;

    /** 
     * Adds an entry to this keyboard translator's table.  Entries can be looked up according
     * to their key sequence using findEntry()
     */
    void addEntry(const Entry& entry);

    /** Returns a list of all entries in the translator. */
    QList<Entry> entries() const;

private:
    QMultiHash<int,Entry> _entries; // entries in this keyboard translation,
                                                 // entries are indexed according to
                                                 // their keycode
    QString _name;
    QString _description;
};

/** 
 * Parses the contents of a Keyboard Translator (.keytab) file and 
 * returns the entries found in it.
 *
 * Usage example:
 *
 * @code
 *  QFile source( "/path/to/keytab" );
 *  source.open( QIODevice::ReadOnly );
 *
 *  KeyboardTranslator* translator = new KeyboardTranslator( "name-of-translator" );
 *
 *  KeyboardTranslatorReader reader(source);
 *  while ( reader.hasNextEntry() )
 *      translator->addEntry(reader.nextEntry());
 *
 *  source.close();
 *
 *  if ( !reader.parseError() )
 *  {
 *      // parsing succeeded, do something with the translator
 *  } 
 *  else
 *  {
 *      // parsing failed
 *  }
 * @endcode
 */
class KeyboardTranslatorReader
{
public:
    /** Constructs a new reader which parses the given @p source */
    KeyboardTranslatorReader( QIODevice* source );

    /** 
     * Returns the description text. 
     * TODO: More documentation 
     */
    QString description() const;

    /** Returns true if there is another entry in the source stream */
    bool hasNextEntry();
    /** Returns the next entry found in the source stream */
    KeyboardTranslator::Entry nextEntry(); 

    /** 
     * Returns true if an error occurred whilst parsing the input or
     * false if no error occurred.
     */
    bool parseError();

private:
    struct Token
    {
        enum Type
        {
            TitleKeyword,
            TitleText,
            KeyKeyword,
            KeySequence,
            Command,
            OutputText
        };
        Type type;
        QString text;
    };
    QList<Token> tokenize(const QString&);
    void readNext();
    QKeySequence decodeSequence(const QString& , KeyboardTranslator::State& stateFlags);

    QIODevice* _source;
    QString _description;
    KeyboardTranslator::Entry _nextEntry;
    bool _hasNext;
};

/**
 * Manages the keyboard translations available for use by terminal sessions,
 * see KeyboardTranslator.
 */
class KeyboardTranslatorManager
{
public:
    /** 
     * Constructs a new KeyboardTranslatorManager and loads the list of
     * available keyboard translations.
     *
     * The keyboard translations themselves are not loaded until they are
     * first requested via a call to findTranslator()
     */
    KeyboardTranslatorManager();

    /**
     * Adds a new translator.  If a translator with the same name 
     * already exists, it will be replaced by the new translator.
     *
     * TODO: More documentation.
     */
    void addTranslator(KeyboardTranslator* translator);

    /**
     * Deletes a translator.
     * TODO: More documentation
     */
    void deleteTranslator(const QString& name);

    /** 
     * Returns the keyboard translator with the given name or 0 if no translator
     * with that name exists.
     *
     * The first time that a translator with a particular name is requested,
     * the on-disk .keyboard file is loaded and parsed.  
     */
    const KeyboardTranslator* findTranslator(const QString& name);
    /**
     * Returns a list of the names of available keyboard translators.
     *
     * The first time this is called, a search for available 
     * translators is started.
     */
    QList<QString> allTranslators();

    /** Sets the global KeyboardTranslatorManager instance. */
   static void setInstance(KeyboardTranslatorManager* instance);
    /** Returns the global KeyboardTranslatorManager instance. */
   static KeyboardTranslatorManager* instance();

private:
    void findTranslators(); // locate the available translators
    KeyboardTranslator* loadTranslator(const QString& name); // loads the translator 
                                                             // with the given name
    
    QString findTranslatorPath(const QString& name);
    
    QHash<QString,KeyboardTranslator*> _translators; // maps translator-name -> KeyboardTranslator
                                                     // instance


    bool _haveLoadedAll;

    static KeyboardTranslatorManager* _instance;
};

inline int KeyboardTranslator::Entry::keyCode() const
{
    return _keyCode;
}
inline Qt::KeyboardModifier KeyboardTranslator::Entry::modifiers() const
{
    return _modifiers;
}
inline KeyboardTranslator::Command KeyboardTranslator::Entry::command() const
{
    return _command;
}
inline QByteArray KeyboardTranslator::Entry::text() const
{
    return _text;
}
inline KeyboardTranslator::State KeyboardTranslator::Entry::state() const
{
    return _state;
}

};

Q_DECLARE_METATYPE(const Konsole::KeyboardTranslator*)

#endif // KEYBOARDTRANSLATOR_H

