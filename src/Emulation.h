/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef EMULATION_H
#define EMULATION_H

// Qt
#include <QSize>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QTimer>

// Konsole
#include "Enumeration.h"
#include "konsoleprivate_export.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <memory>

class QKeyEvent;

namespace Konsole
{
class KeyboardTranslator;
class HistoryType;
class Screen;
class ScreenWindow;
class TerminalCharacterDecoder;

/**
 * Base class for terminal emulation back-ends.
 *
 * The back-end is responsible for decoding an incoming character stream and
 * producing an output image of characters.
 *
 * When input from the terminal is received, the receiveData() slot should be called with
 * the data which has arrived.  The emulation will process the data and update the
 * screen image accordingly.  The codec used to decode the incoming character stream
 * into the unicode characters used internally can be specified using setCodec()
 *
 * The size of the screen image can be specified by calling setImageSize() with the
 * desired number of lines and columns.  When new lines are added, old content
 * is moved into a history store, which can be set by calling setHistory().
 *
 * The screen image can be accessed by creating a ScreenWindow onto this emulation
 * by calling createWindow().  Screen windows provide access to a section of the
 * output.  Each screen window covers the same number of lines and columns as the
 * image size returned by imageSize().  The screen window can be moved up and down
 * and provides transparent access to both the current on-screen image and the
 * previous output.  The screen windows emit an outputChanged signal
 * when the section of the image they are looking at changes.
 * Graphical views can then render the contents of a screen window, listening for notifications
 * of output changes from the screen window which they are associated with and updating
 * accordingly.
 *
 * The emulation also is also responsible for converting input from the connected views such
 * as keypresses and mouse activity into a character string which can be sent
 * to the terminal program.  Key presses can be processed by calling the sendKeyEvent() slot,
 * while mouse events can be processed using the sendMouseEvent() slot.  When the character
 * stream has been produced, the emulation will emit a sendData() signal with a pointer
 * to the character buffer.  This data should be fed to the standard input of the terminal
 * process.  The translation of key presses into an output character stream is performed
 * using a lookup in a set of key bindings which map key sequences to output
 * character sequences.  The name of the key bindings set used can be specified using
 * setKeyBindings()
 *
 * The emulation maintains certain state information which changes depending on the
 * input received.  The emulation can be reset back to its starting state by calling
 * reset().
 *
 * The emulation also maintains an activity state, which specifies whether
 * terminal is currently active ( when data is received ), normal
 * ( when the terminal is idle or receiving user input ) or trying
 * to alert the user ( also known as a "Bell" event ).  The stateSet() signal
 * is emitted whenever the activity state is set.  This can be used to determine
 * how long the emulation has been active/idle for and also respond to
 * a 'bell' event in different ways.
 */
class KONSOLEPRIVATE_EXPORT Emulation : public QObject
{
    Q_OBJECT

public:
    /** Constructs a new terminal emulation */
    Emulation();
    ~Emulation() override;

    /**
     * Creates a new window onto the output from this emulation.  The contents
     * of the window are then rendered by views which are set to use this window using the
     * TerminalDisplay::setScreenWindow() method.
     */
    ScreenWindow *createWindow();

    /**
     * Associates a TerminalDisplay with this emulation.
     */
    void setCurrentTerminalDisplay(TerminalDisplay *display);

    /** Returns the size of the screen image which the emulation produces */
    QSize imageSize() const;

    /**
     * Returns the total number of lines, including those stored in the history.
     */
    int lineCount() const;

    /**
     * Sets the history store used by this emulation.  When new lines
     * are added to the output, older lines at the top of the screen are transferred to a history
     * store.
     *
     * The number of lines which are kept and the storage location depend on the
     * type of store.
     */
    void setHistory(const HistoryType &);
    /** Returns the history store used by this emulation.  See setHistory() */
    const HistoryType &history() const;
    /** Clears the history scroll. */
    virtual void clearHistory();

    /**
     * Copies the output history from @p startLine to @p endLine
     * into @p stream, using @p decoder to convert the terminal
     * characters into text.
     *
     * @param decoder A decoder which converts lines of terminal characters with
     * appearance attributes into output text.  PlainTextDecoder is the most commonly
     * used decoder.
     * @param startLine Index of first line to copy
     * @param endLine Index of last line to copy
     */
    virtual void writeToStream(TerminalCharacterDecoder *decoder, int startLine, int endLine);

    /** Returns the decoder used to decode incoming characters.  See setCodec() */
    const QStringDecoder &decoder() const
    {
        return _decoder;
    }

    /** Returns the encoder used to encode characters send to the terminal.  See setCodec() */
    const QStringEncoder &encoder() const
    {
        return _encoder;
    }

    /** Sets the codec used to decode incoming characters.  */
    bool setCodec(const QByteArray &name);

    /**
     * Convenience method.
     * Returns true if the current codec used to decode incoming
     * characters is UTF-8
     */
    bool utf8() const
    {
        Q_ASSERT(_decoder.isValid());
        const std::optional<QStringConverter::Encoding> encoding = QStringConverter::encodingForName(_decoder.name());
#if defined(Q_OS_WIN)
        return encoding == QStringConverter::Utf8;
#else
        return encoding == QStringConverter::Utf8 || encoding == QStringConverter::System;
#endif
    }

    /** Returns the special character used for erasing character. */
    virtual char eraseChar() const;

    /**
     * Sets the key bindings used to key events
     * ( received through sendKeyEvent() ) into character
     * streams to send to the terminal.
     */
    void setKeyBindings(const QString &name);
    /**
     * Returns the name of the emulation's current key bindings.
     * See setKeyBindings()
     */
    QString keyBindings() const;

    /**
     * Copies the current image into the history and clears the screen.
     */
    virtual void clearEntireScreen() = 0;

    /** Resets the state of the terminal.
     *
     * @param softReset The reset was initiated by DECSTR
     * @param preservePrompt Try to preserve the command prompt */
    virtual void reset(bool softReset = false, bool preservePrompt = false) = 0;

    /**
     * Returns true if the active terminal program is interested in Mouse
     * Tracking events.
     *
     * The programRequestsMouseTracking() signal is emitted when a program
     * indicates it's interested in Mouse Tracking events.
     *
     * See MODE_Mouse100{0,1,2,3} in Vt102Emulation.
     */
    bool programUsesMouseTracking() const;

    bool programBracketedPasteMode() const;

    QList<int> getCurrentScreenCharacterCounts() const;

public Q_SLOTS:

    /** Change the size of the emulation's image */
    virtual void setImageSize(int lines, int columns);

    /**
     * Interprets a sequence of characters and sends the result to the terminal.
     * This is equivalent to calling sendKeyEvent() for each character in @p text in succession.
     */
    virtual void sendText(const QString &text) = 0;

    /**
     * Interprets a key press event and emits the sendData() signal with
     * the resulting character stream.
     */
    virtual void sendKeyEvent(QKeyEvent *);

    /**
     * Converts information about a mouse event into an xterm-compatible escape
     * sequence and emits the character sequence via sendData()
     */
    virtual void sendMouseEvent(int buttons, int column, int line, int eventType) = 0;

    /**
     * Sends a string of characters to the foreground terminal process.
     *
     * @param string The characters to send.
     */
    virtual void sendString(const QByteArray &string) = 0;

    /**
     * Processes an incoming stream of characters.  receiveData() decodes the incoming
     * character buffer using the current codec(), and then calls receiveChar() for
     * each unicode character in the resulting buffer.
     *
     * receiveData() also starts a timer which causes the outputChanged() signal
     * to be emitted when it expires.  The timer allows multiple updates in quick
     * succession to be buffered into a single outputChanged() signal emission.
     *
     * @param text A string of characters received from the terminal program.
     * @param length The length of @p text
     */
    void receiveData(const char *text, int length);

    /**
     * Sends information about the focus event to the terminal.
     */
    virtual void focusChanged(bool focused) = 0;

    void setPeekPrimary(const bool doPeek);

Q_SIGNALS:

    /**
     * Emitted when a buffer of data is ready to send to the
     * standard input of the terminal.
     *
     * @param data The buffer of data ready to be sent
     */
    void sendData(const QByteArray &data);

    /**
     * Requests that the pty used by the terminal process
     * be set to UTF 8 mode.
     *
     * Refer to the IUTF8 entry in termios(3) for more information.
     */
    void useUtf8Request(bool);

    /**
     * Emitted when bell appeared
     */
    void bell();

    /**
     * Emitted when the special sequence indicating the request for data
     * transmission through ZModem protocol is detected.
     */
    void zmodemDownloadDetected();
    void zmodemUploadDetected();

    /**
     * This is emitted when the program (typically editors and other full-screen
     * applications, ones that take up the whole terminal display), running in
     * the terminal indicates whether or not it is interested in Mouse Tracking
     * events. This is an XTerm extension, for more information have a look at:
     * https://invisible-island.net/xterm/ctlseqs/ctlseqs.html#h2-Mouse-Tracking
     *
     * @param usesMouseTracking This will be true if the program is interested
     * in Mouse Tracking events or false otherwise.
     */
    void programRequestsMouseTracking(bool usesMouseTracking);

    void enableAlternateScrolling(bool enable);

    void programBracketedPasteModeChanged(bool bracketedPasteMode);

    /**
     * Emitted when the contents of the screen image change.
     * The emulation buffers the updates from successive image changes,
     * and only emits outputChanged() at sensible intervals when
     * there is a lot of terminal activity.
     *
     * Normally there is no need for objects other than the screen windows
     * created with createWindow() to listen for this signal.
     *
     * ScreenWindow objects created using createWindow() will emit their
     * own outputChanged() signal in response to this signal.
     */
    void outputChanged();

    /**
     * Emitted when the program running in the terminal wishes to update
     * certain session attributes. This allows terminal programs to customize
     * certain aspects of the terminal emulation display.
     *
     * This signal is emitted when the escape sequence "\033]ARG;VALUE\007"
     * is received in an input string, where ARG is a number specifying
     * what should be changed and VALUE is a string specifying the new value.
     *
     * @param attribute Specifies which session attribute to change:
     * <ul>
     * <li> 0  - Set window icon text and session title to @p newValue</li>
     * <li> 1  - Set window icon text to @p newValue</li>
     * <li> 2  - Set session title to @p newValue</li>
     * <li> 11 - Set the session's default background color to @p newValue,
     *      where @p newValue can be an HTML-style string ("#RRGGBB") or a
     *      named color (e.g. 'red', 'blue'). For more details see:
     *      https://doc.qt.io/qt-5/qcolor.html#setNamedColor
     * </li>
     * <li> 31 - Supposedly treats @p newValue as a URL and opens it (NOT
     *      IMPLEMENTED)
     * </li>
     * <li> 32 - Sets the icon associated with the session. @p newValue
     *      is the name of the icon to use, which can be the name of any
     *      icon in the current KDE icon theme (eg: 'konsole', 'kate',
     *      'folder_home')
     * </li>
     * </ul>
     * @param newValue Specifies the new value of the session attribute
     */
    void sessionAttributeChanged(int attribute, const QString &newValue);

    /**
     * Emitted when the terminal emulator's size has changed
     */
    void imageSizeChanged(int lineCount, int columnCount);

    /**
     * Emitted when the setImageSize() is called on this emulation for
     * the first time.
     */
    void imageSizeInitialized();

    /**
     * Emitted after receiving the escape sequence which asks to change
     * the terminal emulator's size
     */
    void imageResizeRequest(const QSize &sizz);

    /**
     * Emitted when the terminal program requests to change various properties
     * of the terminal display.
     *
     * A profile change command occurs when a special escape sequence, followed
     * by a string containing a series of name and value pairs is received.
     * This string can be parsed using a ProfileCommandParser instance.
     *
     * @param text A string expected to contain a series of key and value pairs in
     * the form:  name=value;name2=value2 ...
     */
    void profileChangeCommandReceived(const QString &text);

    /**
     * Emitted when a flow control key combination ( Ctrl+S or Ctrl+Q ) is pressed.
     * @param suspendKeyPressed True if Ctrl+S was pressed to suspend output or Ctrl+Q to
     * resume output.
     */
    void flowControlKeyPressed(bool suspendKeyPressed);

    /**
     * Emitted when the active screen is switched, to indicate whether the primary
     * screen is in use.
     */
    void primaryScreenInUse(bool use);

    /**
     * Emitted when the text selection is changed
     */
    void selectionChanged(const bool selectionEmpty);

    /**
     * Emitted when terminal code requiring terminal's response received.
     */
    void sessionAttributeRequest(int id, uint terminator);

    /**
     * Emitted when Set Cursor Style (DECSCUSR) escape sequences are sent
     * to the terminal.
     * @p shape cursor shape
     * @p isBlinking if true, the cursor will be set to blink
     * @p customColor custom cursor color
     */
    void setCursorStyleRequest(Enum::CursorShapeEnum shape = Enum::BlockCursor, bool isBlinking = false, const QColor &customColor = {});

    /**
     * Emitted when reset() is called to reset the cursor style to the
     * current profile cursor shape and blinking settings.
     */
    void resetCursorStyleRequest();

    void toggleUrlExtractionRequest();

    /**
     * Mainly used to communicate dropped lines to active autosave tasks.
     * Takes into account lines dropped by Screen::addHistLine and Screen::fastAddHistLine.
     * Also includes lines dropped by clearing scrollback and resetting the screen.
     */
    void updateDroppedLines(int droppedLines);

protected:
    virtual void setMode(int mode) = 0;
    virtual void resetMode(int mode) = 0;

    /**
     * Processes an incoming character.  See receiveData()
     * @p c A unicode character code.
     */
    virtual void receiveChars(const QVector<uint> &c);

    /**
     * Sets the active screen.  The terminal has two screens, primary and alternate.
     * The primary screen is used by default.  When certain interactive programs such
     * as Vim are run, they trigger a switch to the alternate screen.
     *
     * @param index 0 to switch to the primary screen, or 1 to switch to the alternate screen
     */
    void setScreen(int index);

    enum EmulationCodec {
        LocaleCodec = 0,
        Utf8Codec = 1,
    };

    void setCodec(EmulationCodec codec);

    QList<ScreenWindow *> _windows;

    Screen *_currentScreen = nullptr; // pointer to the screen which is currently active,
    // this is one of the elements in the screen[] array

    Screen *_screen[2]; // 0 = primary screen ( used by most programs, including the shell
    //                      scrollbars are enabled in this mode )
    // 1 = alternate      ( used by vi , emacs etc.
    //                      scrollbars are not enabled in this mode )

    // decodes an incoming C-style character stream into a unicode QString using
    QStringDecoder _decoder;

    // the current text encoder to send unicode to the terminal
    // (this allows for rendering of non-ASCII characters in text files etc.)
    QStringEncoder _encoder;

    const KeyboardTranslator *_keyTranslator = nullptr; // the keyboard layout

protected Q_SLOTS:
    /**
     * Schedules an update of attached views.
     * Repeated calls to bufferedUpdate() in close succession will result in only a single update,
     * much like the Qt buffered update of widgets.
     */
    void bufferedUpdate();

    // used to emit the primaryScreenInUse(bool) signal
    void checkScreenInUse();

    // used to emit the selectionChanged(QString) signal
    void checkSelectedText();

private Q_SLOTS:
    // triggered by timer, causes the emulation to send an updated screen image to each
    // view
    void showBulk();

    void setUsesMouseTracking(bool usesMouseTracking);

    void bracketedPasteModeChanged(bool bracketedPasteMode);

private:
    void setScreenInternal(int index);
    Q_DISABLE_COPY(Emulation)

    bool _usesMouseTracking = false;
    bool _bracketedPasteMode = false;
    QTimer _bulkTimer1{this};
    QTimer _bulkTimer2{this};
    bool _imageSizeInitialized = false;
    bool _peekingPrimary = false;
    int _activeScreenIndex = 0;
};
}

#endif // ifndef EMULATION_H
