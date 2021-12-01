/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VT102EMULATION_H
#define VT102EMULATION_H

// Qt
#include <QHash>
#include <QPair>
#include <QVector>

// Konsole
#include "Emulation.h"
#include "Screen.h"

class QTimer;
class QKeyEvent;

#define MODE_AppCuKeys (MODES_SCREEN + 0) // Application cursor keys (DECCKM)
#define MODE_AppKeyPad (MODES_SCREEN + 1) //
#define MODE_Mouse1000 (MODES_SCREEN + 2) // Send mouse X,Y position on press and release
#define MODE_Mouse1001 (MODES_SCREEN + 3) // Use Highlight mouse tracking
#define MODE_Mouse1002 (MODES_SCREEN + 4) // Use cell motion mouse tracking
#define MODE_Mouse1003 (MODES_SCREEN + 5) // Use all motion mouse tracking
#define MODE_Mouse1005 (MODES_SCREEN + 6) // Xterm-style extended coordinates
#define MODE_Mouse1006 (MODES_SCREEN + 7) // 2nd Xterm-style extended coordinates
#define MODE_Mouse1007 (MODES_SCREEN + 8) // XTerm Alternate Scroll mode; also check AlternateScrolling profile property
#define MODE_Mouse1015 (MODES_SCREEN + 9) // Urxvt-style extended coordinates
#define MODE_Ansi (MODES_SCREEN + 10) // Use US Ascii for character sets G0-G3 (DECANM)
#define MODE_132Columns (MODES_SCREEN + 11) // 80 <-> 132 column mode switch (DECCOLM)
#define MODE_Allow132Columns (MODES_SCREEN + 12) // Allow DECCOLM mode
#define MODE_BracketedPaste (MODES_SCREEN + 13) // Xterm-style bracketed paste mode
#define MODE_total (MODES_SCREEN + 14)

namespace Konsole
{
extern unsigned short vt100_graphics[32];

struct CharCodes {
    // coding info
    char charset[4]; //
    int cu_cs; // actual charset.
    bool graphic; // Some VT100 tricks
    bool pound; // Some VT100 tricks
    bool sa_graphic; // saved graphic
    bool sa_pound; // saved pound
};

/**
 * Provides an xterm compatible terminal emulation based on the DEC VT102 terminal.
 * A full description of this terminal can be found at https://vt100.net/docs/vt102-ug/
 *
 * In addition, various additional xterm escape sequences are supported to provide
 * features such as mouse input handling.
 * See https://invisible-island.net/xterm/ctlseqs/ctlseqs.html for a description of xterm's escape
 * sequences.
 *
 */
class KONSOLEPRIVATE_EXPORT Vt102Emulation : public Emulation
{
    Q_OBJECT

public:
    /** Constructs a new emulation */
    Vt102Emulation();
    ~Vt102Emulation() override;

    // reimplemented from Emulation
    void clearEntireScreen() override;
    void reset() override;
    char eraseChar() const override;

public Q_SLOTS:
    // reimplemented from Emulation
    void sendString(const QByteArray &string) override;
    void sendText(const QString &text) override;
    void sendKeyEvent(QKeyEvent *) override;
    void sendMouseEvent(int buttons, int column, int line, int eventType) override;
    void focusChanged(bool focused) override;

protected:
    // reimplemented from Emulation
    void setMode(int mode) override;
    void resetMode(int mode) override;
    void receiveChars(const QVector<uint> &chars) override;

private Q_SLOTS:
    // Causes sessionAttributeChanged() to be emitted for each (int,QString)
    // pair in _pendingSessionAttributesUpdates.
    // Used to buffer multiple attribute updates in the current session
    void updateSessionAttributes();

private:
    unsigned int applyCharset(uint c);
    void setCharset(int n, int cs);
    void useCharset(int n);
    void setAndUseCharset(int n, int cs);
    void saveCursor();
    void restoreCursor();
    void resetCharset(int scrno);

    void setMargins(int top, int bottom);
    // set margins for all screens back to their defaults
    void setDefaultMargins();

    // returns true if 'mode' is set or false otherwise
    bool getMode(int mode);
    // saves the current boolean value of 'mode'
    void saveMode(int mode);
    // restores the boolean value of 'mode'
    void restoreMode(int mode);
    // resets all modes
    // (except MODE_Allow132Columns)
    void resetModes();

    void resetTokenizer();
#define MAX_TOKEN_LENGTH 256 // Max length of tokens (e.g. window title)
    void addToCurrentToken(uint cc);
    int tokenBufferPos;
    uint tokenBuffer[MAX_TOKEN_LENGTH]; // FIXME: overflow?
#define MAXARGS 15
    void addDigit(int dig);
    void addArgument();
    int argv[MAXARGS];
    int argc;
    void initTokenizer();

    // Set of flags for each of the ASCII characters which indicates
    // what category they fall into (printable character, control, digit etc.)
    // for the purposes of decoding terminal output
    int charClass[256];

    void reportDecodingError();

    void processToken(int code, int p, int q);
    void processSessionAttributeRequest(int tokenSize);
    void processChecksumRequest(int argc, int argv[]);

    void reportTerminalType();
    void reportTertiaryAttributes();
    void reportSecondaryAttributes();
    void reportStatus();
    void reportAnswerBack();
    void reportCursorPosition();
    void reportSize();
    void reportTerminalParms(int p);

    // clears the screen and resizes it to the specified
    // number of columns
    void clearScreenAndSetColumns(int columnCount);

    CharCodes _charset[2];

    class TerminalState
    {
    public:
        // Initializes all modes to false
        TerminalState()
        {
            memset(&mode, 0, MODE_total * sizeof(bool));
        }

        bool mode[MODE_total];
    };

    TerminalState _currentModes;
    TerminalState _savedModes;

    // Hash table and timer for buffering calls to update certain session
    // attributes (e.g. the name of the session, window title).
    // These calls occur when certain escape sequences are detected in the
    // output from the terminal. See Emulation::sessionAttributeChanged()
    QHash<int, QString> _pendingSessionAttributesUpdates;
    QTimer *_sessionAttributesUpdateTimer;

    bool _reportFocusEvents;
};

}

#endif // VT102EMULATION_H
