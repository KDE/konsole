/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robert.knight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
// Own
#include "Vt102Emulation.h"
#include "config-konsole.h"

// Standard
#include <cstdio>
#include <unistd.h>

// Qt
#include <QEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QtEndian>

// KDE
#include <KLocalizedString>

// Konsole
#include "EscapeSequenceUrlExtractor.h"
#include "session/SessionController.h"
#include "terminalDisplay/TerminalColor.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"

#include <konsoledebug.h>

using Konsole::Vt102Emulation;

/*
   The VT100 has 32 special graphical characters. The usual vt100 extended
   xterm fonts have these at 0x00..0x1f.

   QT's iso mapping leaves 0x00..0x7f without any changes. But the graphicals
   come in here as proper unicode characters.

   We treat non-iso10646 fonts as VT100 extended and do the required mapping
   from unicode to 0x00..0x1f. The remaining translation is then left to the
   QCodec.
*/

// assert for i in [0..31] : vt100extended(vt100_graphics[i]) == i.

/* clang-format off */
unsigned short Konsole::vt100_graphics[32] = {
    // 0/8     1/9    2/10    3/11    4/12    5/13    6/14    7/15
    0x0020, 0x25C6, 0x2592, 0x2409, 0x240c, 0x240d, 0x240a, 0x00b0,
    0x00b1, 0x2424, 0x240b, 0x2518, 0x2510, 0x250c, 0x2514, 0x253c,
    0x23ba, 0x23bb, 0x2500, 0x23bc, 0x23bd, 0x251c, 0x2524, 0x2534,
    0x252c, 0x2502, 0x2264, 0x2265, 0x03C0, 0x2260, 0x00A3, 0x00b7
};
/* clang-format on */

enum XTERM_EXTENDED {
    URL_LINK = '8',
};

Vt102Emulation::Vt102Emulation()
    : Emulation()
    , _currentModes(TerminalState())
    , _savedModes(TerminalState())
    , _pendingSessionAttributesUpdates(QHash<int, QString>())
    , _sessionAttributesUpdateTimer(new QTimer(this))
    , _reportFocusEvents(false)
{
    _sessionAttributesUpdateTimer->setSingleShot(true);
    QObject::connect(_sessionAttributesUpdateTimer, &QTimer::timeout, this, &Konsole::Vt102Emulation::updateSessionAttributes);

    initTokenizer();
    imageId = 0;
    savedKeys = QMap<char, qint64>();
    tokenData = QByteArray();

    for (int i = 0; i < 256; i++) {
        colorTable[i] = QColor();
    }
}

Vt102Emulation::~Vt102Emulation()
{
}

void Vt102Emulation::clearEntireScreen()
{
    _currentScreen->clearEntireScreen();
    bufferedUpdate();
}

void Vt102Emulation::clearHistory()
{
    _graphicsImages.clear();

    Emulation::clearHistory();
}

void Vt102Emulation::reset(bool softReset, bool preservePrompt)
{
    // Save the current codec so we can set it later.
    // Ideally we would want to use the profile setting
    const QTextCodec *currentCodec = codec();

    resetTokenizer();
    if (softReset) {
        resetMode(MODE_AppCuKeys);
        saveMode(MODE_AppCuKeys);
        resetMode(MODE_AppKeyPad);
        saveMode(MODE_AppKeyPad);
    } else {
        resetModes();
    }

    resetCharset(0);
    _screen[0]->reset(softReset, preservePrompt);
    resetCharset(1);
    _screen[1]->reset(softReset, preservePrompt);

    if (currentCodec != nullptr) {
        setCodec(currentCodec);
    } else {
        setCodec(LocaleCodec);
    }

    Q_EMIT resetCursorStyleRequest();

    bufferedUpdate();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                     Processing the incoming byte stream                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/* Incoming Bytes Event pipeline

   This section deals with decoding the incoming character stream.
   Decoding means here, that the stream is first separated into `tokens'
   which are then mapped to a `meaning' provided as operations by the
   `Screen' class or by the emulation class itself.

   The pipeline proceeds as follows:

   - Tokenizing the ESC codes (onReceiveChar)
   - VT100 code page translation of plain characters (applyCharset)
   - Interpretation of ESC codes (processToken)

   The escape codes and their meaning are described in the
   technical reference of this program.
*/

// Tokens ------------------------------------------------------------------ --

/*
   Since the tokens are the central notion if this section, we've put them
   in front. They provide the syntactical elements used to represent the
   terminals operations as byte sequences.

   They are encodes here into a single machine word, so that we can later
   switch over them easily. Depending on the token itself, additional
   argument variables are filled with parameter values.

   The tokens are defined below:

   - CHR        - Printable characters     (32..255 but DEL (=127))
   - CTL        - Control characters       (0..31 but ESC (= 27), DEL)
   - ESC        - Escape codes of the form <ESC><CHR but `[]()+*#'>
   - ESC_DE     - Escape codes of the form <ESC><any of `()+*#%'> C
   - CSI_PN     - Escape codes of the form <ESC>'['     {Pn} ';' {Pn} C
   - CSI_PS     - Escape codes of the form <ESC>'['     {Pn} ';' ...  C
   - CSI_PR     - Escape codes of the form <ESC>'[' '?' {Pn} ';' ...  C
   - CSI_PE     - Escape codes of the form <ESC>'[' '!' {Pn} ';' ...  C
   - CSI_SP     - Escape codes of the form <ESC>'[' ' ' C
                  (3rd field is a space)
   - CSI_PSP    - Escape codes of the form <ESC>'[' '{Pn}' ' ' C
                  (4th field is a space)
   - VT52       - VT52 escape codes
                  - <ESC><Chr>
                  - <ESC>'Y'{Pc}{Pc}
   - XTE_HA     - Xterm window/terminal attribute commands
                  of the form <ESC>`]' {Pn} `;' {Text} <BEL>
                  (Note that these are handled differently to the other formats)

   The last two forms allow list of arguments. Since the elements of
   the lists are treated individually the same way, they are passed
   as individual tokens to the interpretation. Further, because the
   meaning of the parameters are names (although represented as numbers),
   they are includes within the token ('N').

*/
constexpr int token_construct(int t, int a, int n)
{
    return (((n & 0xffff) << 16) | ((a & 0xff) << 8) | (t & 0xff));
}
constexpr int token_chr()
{
    return token_construct(0, 0, 0);
}
constexpr int token_ctl(int a)
{
    return token_construct(1, a, 0);
}
constexpr int token_esc(int a)
{
    return token_construct(2, a, 0);
}
constexpr int token_esc_cs(int a, int b)
{
    return token_construct(3, a, b);
}
constexpr int token_esc_de(int a)
{
    return token_construct(4, a, 0);
}
constexpr int token_csi_ps(int a, int n)
{
    return token_construct(5, a, n);
}
constexpr int token_csi_pn(int a)
{
    return token_construct(6, a, 0);
}
constexpr int token_csi_pr(int a, int n)
{
    return token_construct(7, a, n);
}
constexpr int token_vt52(int a)
{
    return token_construct(8, a, 0);
}
constexpr int token_csi_pg(int a)
{
    return token_construct(9, a, 0);
}
constexpr int token_csi_pe(int a)
{
    return token_construct(10, a, 0);
}
constexpr int token_csi_sp(int a)
{
    return token_construct(11, a, 0);
}
constexpr int token_csi_psp(int a, int n)
{
    return token_construct(12, a, n);
}
constexpr int token_csi_pq(int a)
{
    return token_construct(13, a, 0);
}
constexpr int token_osc(int a)
{
    return token_construct(14, a, 0);
}
constexpr int token_apc(int a)
{
    return token_construct(15, a, 0);
}

const int MAX_ARGUMENT = 40960;

// Tokenizer --------------------------------------------------------------- --

/* The tokenizer's state

   The state is represented by the buffer (tokenBuffer, tokenBufferPos),
   and accompanied by decoded arguments kept in params.
   Note that they are kept internal in the tokenizer.
*/

void Vt102Emulation::resetTokenizer()
{
    tokenBufferPos = 0;
    params.count = 0;
    params.value[0] = 0;
    params.value[1] = 0;
    params.sub[0].value[0] = 0;
    params.sub[0].count = 0;
    params.hasSubParams = false;
    tokenState = -1;
}

void Vt102Emulation::addDigit(int digit)
{
    if (params.sub[params.count].count == 0) {
        params.value[params.count] = qMin(10 * params.value[params.count] + digit, MAX_ARGUMENT);
    } else {
        struct subParam *sub = &params.sub[params.count];
        sub->value[sub->count] = qMin(10 * sub->value[sub->count] + digit, MAX_ARGUMENT);
    }
}

void Vt102Emulation::addArgument()
{
    params.count = qMin(params.count + 1, MAXARGS - 1);
    params.value[params.count] = 0;
    params.sub[params.count].value[0] = 0;
    params.sub[params.count].count = 0;
}

void Vt102Emulation::addSub()
{
    struct subParam *sub = &params.sub[params.count];
    sub->count = qMin(sub->count + 1, MAXARGS - 1);
    sub->value[sub->count] = 0;
    params.hasSubParams = true;
}

void Vt102Emulation::addToCurrentToken(uint cc)
{
    tokenBufferPos = qMin(tokenBufferPos, MAX_TOKEN_LENGTH - 1);
    tokenBuffer[tokenBufferPos] = cc;
    tokenBufferPos++;
}

// Character Class flags used while decoding
const int CTL = 1; // Control character
const int CHR = 2; // Printable character
const int CPN = 4; // TODO: Document me
const int DIG = 8; // Digit
const int SCS = 16; // Select Character Set
const int GRP = 32; // TODO: Document me
const int CPS = 64; // Character which indicates end of window resize
const int INT = 128; // Intermediate Byte (ECMA 48 5.4 -> CSI P..P I..I F)

void Vt102Emulation::initTokenizer()
{
    int i;
    quint8 *s;
    for (i = 0; i < 256; ++i) {
        charClass[i] = 0;
    }
    for (i = 0; i < 32; ++i) {
        charClass[i] |= CTL;
    }
    for (i = 32; i < 256; ++i) {
        charClass[i] |= CHR;
    }
    for (i = 0x20; i < 0x30; ++i) {
        charClass[i] |= INT;
    }
    for (s = (quint8 *)"@ABCDEFGHILMPSTXZbcdfry"; *s != 0U; ++s) {
        charClass[*s] |= CPN;
    }
    // resize = \e[8;<row>;<col>t
    for (s = (quint8 *)"t"; *s != 0U; ++s) {
        charClass[*s] |= CPS;
    }
    for (s = (quint8 *)"0123456789"; *s != 0U; ++s) {
        charClass[*s] |= DIG;
    }
    for (s = (quint8 *)"()+*%"; *s != 0U; ++s) {
        charClass[*s] |= SCS;
    }
    for (s = (quint8 *)"()+*#[]%"; *s != 0U; ++s) {
        charClass[*s] |= GRP;
    }

    resetTokenizer();
}

/* Ok, here comes the nasty part of the decoder.

   Instead of keeping an explicit state, we deduce it from the
   token scanned so far. It is then immediately combined with
   the current character to form a scanning decision.

   This is done by the following defines.

   - P is the length of the token scanned so far.
   - L (often P-1) is the position on which contents we base a decision.
   - C is a character or a group of characters (taken from 'charClass').

   - 'cc' is the current character
   - 's' is a pointer to the start of the token buffer
   - 'p' is the current position within the token buffer

   Note that they need to applied in proper order.
*/

/* clang-format off */
#define lec(P,L,C) (p == (P) && s[(L)] == (C))
#define les(P,L,C) (p == (P) && s[L] < 256 && (charClass[s[(L)]] & (C)) == (C))
#define eec(C)     (p >=  3  && cc == (C))
#define ccc(C)     (cc < 256 && (charClass[cc] & (C)) == (C))
#define ees(C)     (p >=  3  && cc < 256 && (charClass[cc] & (C)) == (C))
#define eps(C)     (p >=  3  && s[2] != '?' && s[2] != '!' && s[2] != '=' && s[2] != '>' && cc < 256 && (charClass[cc] & (C)) == (C) && (charClass[s[p-2]] & (INT)) != (INT) )
#define epp( )     (p >=  3  && s[2] == '?')
#define epe( )     (p >=  3  && s[2] == '!')
#define eeq( )     (p >=  3  && s[2] == '=')
#define egt( )     (p >=  3  && s[2] == '>')
#define esp( )     (p >=  4  && s[2] == SP )
#define epsp( )    (p >=  5  && s[3] == SP )
#define osc        (tokenBufferPos >= 2 && tokenBuffer[1] == ']')
//#define apc(C)     (p >= 3 && s[1] == '_' && s[2] == C)
#define pm         (tokenBufferPos >= 2 && tokenBuffer[1] == '^')
#define apc        (tokenBufferPos >= 2 && tokenBuffer[1] == '_')
#define ces(C)     (cc < 256 && (charClass[cc] & (C)) == (C))
#define dcs        (p >= 2   && s[0] == ESC && s[1] == 'P')
#define sixel( )   (p == 1 && cc >= '?' && cc <= '~')

/* clang-format on */

#define CNTL(c) ((c) - '@')
const int ESC = 27;
const int DEL = 127;
const int SP = 32;

// process an incoming unicode character
//
// Parser based on the vt100.net diagram:
// Williams, Paul Flo. “A parser for DEC’s ANSI-compatible video
// terminals.” VT100.net. https://vt100.net/emu/dec_ansi_parser

void Vt102Emulation::switchState(const ParserStates newState, const uint cc)
{
    switch (_state) {
    case DcsPassthrough:
        unhook();
        break;
    case OscString:
        osc_end(cc);
        break;
    case SosPmApcString:
        apc_end();
        break;
    default:
        break;
    }

    _state = newState;
}

void Vt102Emulation::esc_dispatch(const uint cc)
{
    if (_ignore)
        return;
    if (_nIntermediate == 0) {
        processToken(token_esc(cc), 0, 0);
    } else if (_nIntermediate == 1) {
        const uint intermediate = _intermediate[0];
        if ((charClass[intermediate] & SCS) == SCS) {
            processToken(token_esc_cs(intermediate, cc), 0, 0);
        } else if (intermediate == '#') {
            processToken(token_esc_de(cc), 0, 0);
        }
    }
}

void Vt102Emulation::clear()
{
    _nIntermediate = 0;
    _ignore = false;
    resetTokenizer();
}

#define MAX_INTERMEDIATES 1
void Vt102Emulation::collect(const uint cc)
{
    addToCurrentToken(cc);
    if (cc > 0x30)
        return;
    if (_nIntermediate >= MAX_INTERMEDIATES) {
        _ignore = true;
        return;
    }
    _intermediate[_nIntermediate++] = cc;
}

void Vt102Emulation::param(const uint cc)
{
    addToCurrentToken(cc);
    if ((charClass[cc] & DIG) == DIG) {
        addDigit(cc - '0');
    } else if (cc == ';') {
        addArgument();
    } else if (cc == ':') {
        addSub();
    }
}

void Vt102Emulation::csi_dispatch(const uint cc)
{
    if (_ignore || (params.hasSubParams && cc != 'm')) // Be conservative for now
        return;
    if ((tokenBufferPos == 0 || (tokenBuffer[0] != '?' && tokenBuffer[0] != '!' && tokenBuffer[0] != '=' && tokenBuffer[0] != '>')) && cc < 256
        && (charClass[cc] & CPN) == CPN && _nIntermediate == 0) {
        processToken(token_csi_pn(cc), params.value[0], params.value[1]);
    } else if ((tokenBufferPos == 0 || (tokenBuffer[0] != '?' && tokenBuffer[0] != '!' && tokenBuffer[0] != '=' && tokenBuffer[0] != '>')) && cc < 256
               && (charClass[cc] & CPS) == CPS && _nIntermediate == 0) {
        processToken(token_csi_ps(cc, params.value[0]), params.value[1], params.value[2]);
    } else if (tokenBufferPos != 0 && tokenBuffer[0] == '!') {
        processToken(token_csi_pe(cc), 0, 0);
    } else if (_nIntermediate == 1 && _intermediate[0] == ' ') {
        if (tokenBufferPos == 1) {
            processToken(token_csi_sp(cc), 0, 0);
        } else {
            processToken(token_csi_psp(cc, params.value[0]), 0, 0);
        }
    } else if (cc == 'y' && _nIntermediate == 1 && _intermediate[0] == '*') {
        processChecksumRequest(params.count, params.value);
    } else {
        for (int i = 0; i <= params.count; i++) {
            if (tokenBufferPos != 0 && tokenBuffer[0] == '?') {
                processToken(token_csi_pr(cc, params.value[i]), i, 0);
            } else if (tokenBufferPos != 0 && tokenBuffer[0] == '=') {
                processToken(token_csi_pq(cc), 0, 0);
            } else if (tokenBufferPos != 0 && tokenBuffer[0] == '>') {
                processToken(token_csi_pg(cc), 0, 0);
            } else if (cc == 'm' && !params.sub[i].count && params.count - i >= 4 && (params.value[i] == 38 || params.value[i] == 48)
                       && params.value[i + 1] == 2) {
                // ESC[ ... 48;2;<red>;<green>;<blue> ... m -or- ESC[ ... 38;2;<red>;<green>;<blue> ... m
                i += 2;
                processToken(token_csi_ps(cc, params.value[i - 2]),
                             COLOR_SPACE_RGB,
                             (params.value[i] << 16) | (params.value[i + 1] << 8) | params.value[i + 2]);
                i += 2;
            } else if (cc == 'm' && params.sub[i].count >= 5 && (params.value[i] == 38 || params.value[i] == 48) && params.sub[i].value[1] == 2) {
                // ESC[ ... 48:2:<id>:<red>:<green>:<blue> ... m -or- ESC[ ... 38:2:<id>:<red>:<green>:<blue> ... m
                processToken(token_csi_ps(cc, params.value[i]),
                             COLOR_SPACE_RGB,
                             (params.sub[i].value[3] << 16) | (params.sub[i].value[4] << 8) | params.sub[i].value[5]);
            } else if (cc == 'm' && params.sub[i].count == 4 && (params.value[i] == 38 || params.value[i] == 48) && params.sub[i].value[1] == 2) {
                // ESC[ ... 48:2:<red>:<green>:<blue> ... m -or- ESC[ ... 38:2:<red>:<green>:<blue> ... m
                processToken(token_csi_ps(cc, params.value[i]),
                             COLOR_SPACE_RGB,
                             (params.sub[i].value[2] << 16) | (params.sub[i].value[3] << 8) | params.sub[i].value[4]);
            } else if (cc == 'm' && !params.sub[i].count && params.count - i >= 2 && (params.value[i] == 38 || params.value[i] == 48)
                       && params.value[i + 1] == 5) {
                // ESC[ ... 48;5;<index> ... m -or- ESC[ ... 38;5;<index> ... m
                i += 2;
                processToken(token_csi_ps(cc, params.value[i - 2]), COLOR_SPACE_256, params.value[i]);
            } else if (cc == 'm' && params.sub[i].count >= 2 && (params.value[i] == 38 || params.value[i] == 48) && params.sub[i].value[1] == 5) {
                // ESC[ ... 48:5:<index> ... m -or- ESC[ ... 38:5:<index> ... m
                processToken(token_csi_ps(cc, params.value[i]), COLOR_SPACE_256, params.sub[i].value[2]);
            } else if (_nIntermediate == 0) {
                processToken(token_csi_ps(cc, params.value[i]), 0, 0);
            }
        }
    }
}

void Vt102Emulation::osc_start()
{
    tokenBufferPos = 0;
}

void Vt102Emulation::osc_put(const uint cc)
{
    addToCurrentToken(cc);

    // Special case: iterm file protocol is a long escape sequence
    if (tokenState == -1) {
        tokenStateChange = "1337;File=:";
        tokenState = 0;
    }
    if (tokenState >= 0) {
        if ((uint)tokenStateChange[tokenState] == tokenBuffer[tokenBufferPos - 1]) {
            tokenState++;
            tokenPos = tokenBufferPos;
            if ((uint)tokenState == strlen(tokenStateChange)) {
                tokenState = -2;
                tokenData.clear();
            }
            return;
        }
    } else if (tokenState == -2) {
        if (tokenBufferPos - tokenPos == 4) {
            tokenData.append(QByteArray::fromBase64(QString::fromUcs4(&tokenBuffer[tokenPos], 4).toLocal8Bit()));
            tokenBufferPos -= 4;
            return;
        }
    }
}

void Vt102Emulation::osc_end(const uint cc)
{
    // This runs two times per link, the first prepares the link to be read,
    // the second finalizes it. The escape sequence is in two parts
    //  start: '\e ] 8 ; <id-path> ; <url-part> \e \\'
    //  end:   '\e ] 8 ; ; \e \\'
    // GNU libtextstyle inserts the IDs, for instance; many examples
    // do not.
    if (tokenBuffer[0] == XTERM_EXTENDED::URL_LINK) {
        // printf '\e]8;;https://example.com\e\\This is a link\e]8;;\e\\\n'
        emit toggleUrlExtractionRequest();
    }

    processSessionAttributeRequest(tokenBufferPos, cc);
}

void Vt102Emulation::put(const uint cc)
{
    if (m_SixelPictureDefinition && cc >= 0x21) {
        addToCurrentToken(cc);
        processSixel(cc);
    }
}

void Vt102Emulation::hook(const uint cc)
{
    if (cc == 'q' && _nIntermediate == 0) {
        m_SixelPictureDefinition = true;
        resetTokenizer();
    }
}

void Vt102Emulation::unhook()
{
    m_SixelPictureDefinition = false;
    SixelModeDisable();
    resetTokenizer();
}

void Vt102Emulation::apc_start(const uint cc)
{
    tokenBufferPos = 0;
    if (cc == 0x9F || cc == 0x5F) {
        _sosPmApc = Apc;
    } else if (cc == 0x9E || cc == 0x5E) {
        _sosPmApc = Pm;
    } else {
        // 0x98, 0x58
        _sosPmApc = Sos;
    }
}

void Vt102Emulation::apc_put(const uint cc)
{
    if (_sosPmApc != Apc) {
        return;
    }

    addToCurrentToken(cc);

    // <ESC> '_' ... <ESC> '\'
    if (tokenBufferPos > 1 && tokenBuffer[0] == 'G') {
        if (tokenState == -1) {
            tokenStateChange = ";";
            tokenState = 0;
        } else if (tokenState >= 0) {
            if ((uint)tokenStateChange[tokenState] == tokenBuffer[tokenBufferPos - 1]) {
                tokenState++;
                tokenPos = tokenBufferPos;
                if ((uint)tokenState == strlen(tokenStateChange)) {
                    tokenState = -2;
                    tokenData.clear();
                }
            }
        } else if (tokenState == -2) {
            if (tokenBufferPos - tokenPos == 4) {
                tokenData.append(QByteArray::fromBase64(QString::fromUcs4(&tokenBuffer[tokenPos], 4).toLocal8Bit()));
                tokenBufferPos -= 4;
            }
        }
    }
}

void Vt102Emulation::apc_end()
{
    if (_sosPmApc == Apc && tokenBuffer[0] == 'G') {
        // Graphics command
        processGraphicsToken(tokenBufferPos);
        resetTokenizer();
    }
}

void Vt102Emulation::receiveChars(const QVector<uint> &chars)
{
    for (uint cc : chars) {
        // early out for displayable characters
        if (_state == Ground && ((cc >= 0x20 && cc <= 0x7E) || cc >= 0xA0)) {
            _currentScreen->displayCharacter(applyCharset(cc));
            continue;
        }

        if (getMode(MODE_Ansi)) {
            // First, process characters that act the same on all states, i.e.
            // coming from "anywhere" in the VT100.net diagram.
            if (cc == 0x1B) {
                switchState(Escape, cc);
                clear();
            } else if (cc == 0x9B) {
                switchState(CsiEntry, cc);
                clear();
            } else if (cc == 0x90) {
                switchState(DcsEntry, cc);
                clear();
            } else if (cc == 0x9D) {
                osc_start();
                switchState(OscString, cc);
            } else if (cc == 0x98 || cc == 0x9E || cc == 0x9F) {
                apc_start(cc);
                switchState(SosPmApcString, cc);
            } else if (cc == 0x18 || cc == 0x1A || (cc >= 0x80 && cc <= 0x9A)) { // 0x90, 0x98 taken care of just above.
                // konsole has always ignored CAN and SUB in OSC, extend the behavior a bit
                // this differs from VT240, where 7-bit ST, 8-bit ST, ESC + chr, ***CAN, SUB, C1*** terminate and show SIXEL
                if (_state != OscString && _state != SosPmApcString && _state != DcsPassthrough) {
                    processToken(token_ctl(cc + '@'), 0, 0);
                    switchState(Ground, cc);
                }
            } else if (cc == 0x9C) {
                // no action
                switchState(Ground, cc);

            } else {
                // Now take the current state into account
                switch (_state) {
                case Ground:
                    if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else {
                        // 0x7F is ignored by displayCharacter(), since its Character::width() is -1
                        _currentScreen->displayCharacter(applyCharset(cc));
                    }
                    break;
                case Escape:
                    if (cc == 0x5B) {
                        switchState(CsiEntry, cc);
                        clear();
                    } else if ((cc >= 0x30 && cc <= 0x4F) || (cc >= 0x51 && cc <= 0x57) || (cc >= 0x59 && cc <= 0x5A) || cc == 0x5C
                               || (cc >= 0x60 && cc <= 0x7E)) {
                        esc_dispatch(cc);
                        switchState(Ground, cc);
                    } else if (cc >= 0x20 && cc <= 0x2F) {
                        collect(cc);
                        switchState(EscapeIntermediate, cc);
                    } else if (cc == 0x5D) {
                        osc_start();
                        switchState(OscString, cc);
                    } else if (cc == 0x50) {
                        switchState(DcsEntry, cc);
                        clear();
                    } else if (cc == 0x58 || cc == 0x5E || cc == 0x5F) {
                        apc_start(cc);
                        switchState(SosPmApcString, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case EscapeIntermediate:
                    if (cc >= 0x30 && cc <= 0x7E) {
                        esc_dispatch(cc);
                        switchState(Ground, cc);
                    } else if (cc >= 0x20 && cc <= 0x2F) {
                        collect(cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case CsiEntry:
                    if (cc >= 0x40 && cc <= 0x7E) {
                        csi_dispatch(cc);
                        switchState(Ground, cc);
                    } else if (cc >= 0x30 && cc <= 0x3B) { // recognize 0x3A as part of params
                        param(cc);
                        switchState(CsiParam, cc);
                    } else if (cc >= 0x3C && cc <= 0x3F) {
                        collect(cc);
                        switchState(CsiParam, cc);
                    } else if (cc >= 0x20 && cc <= 0x2F) {
                        collect(cc);
                        switchState(CsiIntermediate, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case CsiParam:
                    if (cc >= 0x40 && cc <= 0x7E) {
                        csi_dispatch(cc);
                        switchState(Ground, cc);
                    } else if (cc >= 0x30 && cc <= 0x3B) { // recognize 0x3A as part of params
                        param(cc);
                    } else if (cc >= 0x3C && cc <= 0x3F) {
                        switchState(CsiIgnore, cc);
                    } else if (cc >= 0x20 && cc <= 0x2F) {
                        collect(cc);
                        switchState(CsiIntermediate, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case CsiIntermediate:
                    if (cc >= 0x40 && cc <= 0x7E) {
                        csi_dispatch(cc);
                        switchState(Ground, cc);
                    } else if (cc >= 0x20 && cc <= 0x2F) {
                        collect(cc);
                    } else if (cc >= 0x30 && cc <= 0x3F) {
                        switchState(CsiIgnore, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case CsiIgnore:
                    if (cc >= 0x40 && cc <= 0x7E) {
                        switchState(Ground, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if ((/*cc >= 0x20 && */ cc <= 0x3F) || cc == 0x7F) { // cc <= 0x1F taking care of just above
                        // ignore
                    }
                    break;
                case DcsEntry:
                    if (cc >= 0x40 && cc <= 0x7E) {
                        hook(cc);
                        switchState(DcsPassthrough, cc);
                    } else if (cc >= 0x30 && cc <= 0x3B) { // recognize 0x3A as part of params
                        param(cc);
                        switchState(DcsParam, cc);
                    } else if (cc >= 0x3C && cc <= 0x3F) {
                        collect(cc);
                        switchState(DcsParam, cc);
                    } else if (cc >= 0x20 && cc <= 0x2F) {
                        collect(cc);
                        switchState(DcsIntermediate, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case DcsParam:
                    if (cc >= 0x40 && cc <= 0x7E) {
                        hook(cc);
                        switchState(DcsPassthrough, cc);
                    } else if (cc >= 0x30 && cc <= 0x3B) { // recognize 0x3A as part of params
                        param(cc);
                    } else if (cc >= 0x3C && cc <= 0x3F) {
                        switchState(DcsIgnore, cc);
                    } else if (cc >= 0x20 && cc <= 0x2F) {
                        collect(cc);
                        switchState(DcsIntermediate, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case DcsIntermediate:
                    if (cc >= 0x40 && cc <= 0x7E) {
                        hook(cc);
                        switchState(DcsPassthrough, cc);
                    } else if (cc >= 0x20 && cc <= 0x2F) {
                        collect(cc);
                    } else if (cc >= 0x30 && cc <= 0x3F) {
                        switchState(DcsIgnore, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of.
                        processToken(token_ctl(cc + '@'), 0, 0);
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case DcsPassthrough:
                    if (cc <= 0x7E || cc >= 0xA0) { // 0x18, 0x1A, 0x1B already taken care of
                        put(cc);
                    // 0x9C already taken care of.
                    } else if (cc == 0x7F) {
                        // ignore
                    }
                    break;
                case DcsIgnore:
                    // 0x9C already taken care of.
                    if (cc <= 0x7F) {
                        // ignore
                    }
                    break;
                case OscString:
                    if ((cc >= 0x20 && cc <= 0x7F) || cc >= 0xA0) {
                        osc_put(cc);
                    } else if (cc == 0x07 // recognize BEL as OSC terminator
                               || cc == 0x9C) {
                        switchState(Ground, cc);
                    } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B already taken care of. 0x07 taken care of just above.
                        // ignore
                    }
                    break;
                case SosPmApcString:
                    if (cc <= 0x7F || cc >= 0xA0) { // 0x18, 0x1A, 0x1B already taken care of.
                        apc_put(cc); // while the vt100.net diagram has ignore here, konsole does process some APCs (kitty images).
                    }
                    // 0x9C already taken care of.
                    break;
                default:
                    break;
                }
            }
        } else {
            // VT52 Mode

            // First, process characters that act the same on all states
            if (cc == 0x18 || cc == 0x1A) {
                processToken(token_ctl(cc + '@'), 0, 0);
                switchState(Ground, cc);
            } else if (cc == 0x1B) {
                switchState(Vt52Escape, cc);
            } else if (cc <= 0x1F) { // 0x18, 0x1A, 0x1B taken care of just above
                processToken(token_ctl(cc + '@'), 0, 0);
            } else {
                // Now take the current state into account
                switch (_state) {
                case Ground:
                    _currentScreen->displayCharacter(applyCharset(cc));
                    break;
                case Vt52Escape:
                    if (cc == 'Y') {
                        switchState(Vt52CupRow, cc);
                    } else if ((cc >= 0x20 && cc <= 'X') || (cc >= 'Z' && cc <= 0x7F)) {
                        processToken(token_vt52(cc), 0, 0);
                        switchState(Ground, cc);
                    }
                    break;
                case Vt52CupRow:
                    tokenBuffer[0] = cc;
                    switchState(Vt52CupColumn, cc);
                    break;
                case Vt52CupColumn:
                    processToken(token_vt52('Y'), tokenBuffer[0], cc);
                    switchState(Ground, cc);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

void Vt102Emulation::processChecksumRequest([[maybe_unused]] int crargc, int crargv[])
{
    int checksum = 0;

#if ENABLE_DECRQCRA
    int top, left, bottom, right;

    /* DEC STD-070 5-179 "If Pp is 0 or omitted, subsequent parameters are ignored
     *  and a checksum for all page memory will be reported."
     */
    if (crargv[1] == 0) {
        crargc = 1;
    }
    /* clang-format off */
    if (crargc >= 2) { top    = crargv[2]; } else { top    = 1; }
    if (crargc >= 3) { left   = crargv[3]; } else { left   = 1; }
    if (crargc >= 4) { bottom = crargv[4]; } else { bottom = _currentScreen->getLines();   }
    if (crargc >= 5) { right  = crargv[5]; } else { right  = _currentScreen->getColumns(); }
    /* clang-format on */

    if (top > bottom || left > right) {
        return;
    }

    if (_currentScreen->getMode(MODE_Origin)) {
        top += _currentScreen->topMargin();
        bottom += _currentScreen->topMargin();
    }

    top = qBound(1, top, _currentScreen->getLines());
    bottom = qBound(1, bottom, _currentScreen->getLines());

    int imgsize = sizeof(Character) * _currentScreen->getLines() * _currentScreen->getColumns();
    Character *image = (Character *)malloc(imgsize);
    if (image == nullptr) {
        fprintf(stderr, "couldn't alloc mem\n");
        return;
    }
    _currentScreen->getImage(image, imgsize, _currentScreen->getHistLines(), _currentScreen->getHistLines() + _currentScreen->getLines() - 1);

    for (int y = top - 1; y <= bottom - 1; y++) {
        for (int x = left - 1; x <= right - 1; x++) {
            // XXX: Apparently, VT520 uses 0x00 for uninitialized cells, konsole can't tell uninitialized cells from spaces
            Character c = image[y * _currentScreen->getColumns() + x];

            if (c.rendition & RE_CONCEAL) {
                checksum += 0x20; // don't reveal secrets
            } else {
                checksum += c.character;
            }

            checksum += (c.rendition & RE_BOLD) / RE_BOLD * 0x80;
            checksum += (c.rendition & RE_BLINK) / RE_BLINK * 0x40;
            checksum += (c.rendition & RE_REVERSE) / RE_REVERSE * 0x20;
            checksum += (c.rendition & RE_UNDERLINE) / RE_UNDERLINE * 0x10;
        }
    }

    free(image);
#endif

    char tmp[30];
    checksum = -checksum;
    checksum &= 0xffff;
    snprintf(tmp, sizeof(tmp), "\033P%d!~%04X\033\\", crargv[0], checksum);
    sendString(tmp);
}

void Vt102Emulation::processSessionAttributeRequest(const int tokenSize, const uint terminator)
{
    // Describes the window or terminal session attribute to change
    // See Session::SessionAttributes for possible values
    int attribute = 0;
    int i;

    /* clang-format off */
    for (i = 0; i < tokenSize &&
                tokenBuffer[i] >= '0'  &&
                tokenBuffer[i] <= '9'; i++)
    {
        attribute = 10 * attribute + (tokenBuffer[i]-'0');
    }
    /* clang-format on */

    if (tokenBuffer[i] != ';') {
        reportDecodingError(token_osc(terminator));
        return;
    }
    // skip initial ';'
    ++i;

    QString value = QString::fromUcs4(&tokenBuffer[i], tokenSize - i);
    if (_currentScreen->urlExtractor() && _currentScreen->urlExtractor()->reading()) {
        // To handle '\e ] 8 ; <id-part> ; <url-part>' we discard
        // the <id-part>. Often it is empty, but GNU libtextstyle
        // may output an id here, see e.g.
        // https://www.gnu.org/software/gettext/libtextstyle/manual/libtextstyle.html#index-styled_005fostream_005fset_005fhyperlink
        value.remove(0, value.indexOf(QLatin1Char(';')) + 1);
        _currentScreen->urlExtractor()->setUrl(value);
        return;
    }

    if (attribute == 133) {
        if (value[0] == L'A' || value[0] == L'N' || value[0] == L'P') {
            _currentScreen->setReplMode(REPL_PROMPT);
        }
        if (value[0] == L'L' && _currentScreen->getCursorX() > 0) {
            _currentScreen->nextLine();
        }
        if (value[0] == L'B') {
            _currentScreen->setReplMode(REPL_INPUT);
        }
        if (value[0] == L'C') {
            _currentScreen->setReplMode(REPL_OUTPUT);
        }
        if (value[0] == L'D') {
            _currentScreen->setReplMode(REPL_None);
        }
    }
    if (attribute == 4) {
        // RGB colors
        QStringList params = value.split(QLatin1Char(';'));
        for (int j = 0; j < params.length(); j += 2) {
            if (params.length() == j + 1) {
                return;
            }
            int c = params[j].toInt();
            if (params[j + 1] == QLatin1String("?")) {
                QColor color = colorTable[c];
                if (!color.isValid()) {
                    color = CharacterColor(COLOR_SPACE_256, c).color(ColorScheme::defaultTable);
                }
                reportColor(c, color);
                return;
            }
            QColor col(params[1]);
            colorTable[c] = col;
        }
        return;
    }
    if (attribute == 104) {
        // RGB colors
        QStringList params = value.split(QLatin1Char(';'));
        for (int k = 0; k < params.length(); k++) {
            int c = params[k].toInt();
            colorTable[c] = QColor();
        }
    }

    if (value == QLatin1String("?")) {
        // pass terminator type indication here, because OSC response terminator
        // should match the terminator of OSC request.
        Q_EMIT sessionAttributeRequest(attribute, terminator);
        return;
    }

    if (attribute == Session::ProfileChange) {
        if (value.startsWith(QLatin1String("CursorShape="))) {
            const auto numStr = QStringView(value).right(1);
            const Enum::CursorShapeEnum shape = static_cast<Enum::CursorShapeEnum>(numStr.toInt());
            Q_EMIT setCursorStyleRequest(shape);
            return;
        }
    }

    if (attribute == 1337) {
        if (value.startsWith(QLatin1String("ReportCellSize"))) {
            iTermReportCellSize();
            return;
        }
        if (!value.startsWith(QLatin1String("File="))) {
            return;
        }
        int pos = value.indexOf(QLatin1Char(':'));
        QStringList params = value.mid(5, pos - 5).split(QLatin1Char(';'));
        int keepAspect = 1;
        int scaledWidth = 0;
        int scaledHeight = 0;
        bool moveCursor = true;
        for (const auto &p : params) {
            int eq = p.indexOf(QLatin1Char('='));
            if (eq > 0) {
                QString var = p.mid(0, eq);
                QString val = p.mid(eq + 1);
                if (var == QLatin1String("inline")) {
                    if (val != QLatin1String("1")) {
                        return;
                    }
                }
                if (var == QLatin1String("preserveAspectRatio")) {
                    if (val == QLatin1String("0")) {
                        keepAspect = 0;
                    }
                }
                if (var == QLatin1String("doNotMoveCursor")) {
                    if (val == QLatin1String("1")) {
                        moveCursor = false;
                    }
                }
                if (var == QLatin1String("width")) {
                    int unitPos = val.toStdString().find_first_not_of("0123456789");
                    scaledWidth = QStringView(val).mid(0, unitPos).toInt();
                    if (unitPos == -1) {
                        scaledWidth *= _currentScreen->currentTerminalDisplay()->terminalFont()->fontWidth();
                    } else {
                        if (val.mid(unitPos) == QLatin1String("%")) {
                            scaledWidth *= _currentScreen->currentTerminalDisplay()->terminalFont()->fontWidth() * _currentScreen->getColumns() / 100;
                        }
                    }
                }
                if (var == QLatin1String("height")) {
                    int unitPos = val.toStdString().find_first_not_of("0123456789");
                    scaledHeight = QStringView(val).mid(0, unitPos).toInt();
                    if (unitPos == -1) {
                        scaledHeight *= _currentScreen->currentTerminalDisplay()->terminalFont()->fontHeight();
                    } else {
                        if (val.mid(unitPos) == QLatin1String("%")) {
                            scaledHeight *= _currentScreen->currentTerminalDisplay()->terminalFont()->fontHeight() * _currentScreen->getLines() / 100;
                        }
                    }
                }
            }
        }

        QPixmap pixmap;
        pixmap.loadFromData(tokenData);
        tokenData.clear();
        if (pixmap.isNull()) {
            return;
        }
        if (scaledWidth && scaledHeight) {
            pixmap = pixmap.scaled(scaledWidth, scaledHeight, (Qt::AspectRatioMode)keepAspect);
        } else {
            if (keepAspect && scaledWidth) {
                pixmap = pixmap.scaledToWidth(scaledWidth);
            } else if (keepAspect && scaledHeight) {
                pixmap = pixmap.scaledToHeight(scaledHeight);
            }
        }
        int rows = -1, cols = -1;
        _currentScreen->addPlacement(pixmap, rows, cols, -1, -1, true, moveCursor);
    }
    _pendingSessionAttributesUpdates[attribute] = value;
    _sessionAttributesUpdateTimer->start(20);
}

void Vt102Emulation::updateSessionAttributes()
{
    QListIterator<int> iter(_pendingSessionAttributesUpdates.keys());
    while (iter.hasNext()) {
        int arg = iter.next();
        Q_EMIT sessionAttributeChanged(arg, _pendingSessionAttributesUpdates[arg]);
    }
    _pendingSessionAttributesUpdates.clear();
}

// Interpreting Codes ---------------------------------------------------------

/*
   Now that the incoming character stream is properly tokenized,
   meaning is assigned to them. These are either operations of
   the current _screen, or of the emulation class itself.

   The token to be interpreted comes in as a machine word
   possibly accompanied by two parameters.

   Likewise, the operations assigned to, come with up to two
   arguments. One could consider to make up a proper table
   from the function below.

   The technical reference manual provides more information
   about this mapping.
*/

void Vt102Emulation::processToken(int token, int p, int q)
{
    /* clang-format off */
    switch (token) {
    case token_chr(         ) :
        _currentScreen->displayCharacter     (p         ); break; //UTF16

    //             127 DEL    : ignored on input

    case token_ctl('@'      ) : /* NUL: ignored                      */ break;
    case token_ctl('A'      ) : /* SOH: ignored                      */ break;
    case token_ctl('B'      ) : /* STX: ignored                      */ break;
    case token_ctl('C'      ) : /* ETX: ignored                      */ break;
    case token_ctl('D'      ) : /* EOT: ignored                      */ break;
    case token_ctl('E'      ) :      reportAnswerBack     (          ); break; //VT100
    case token_ctl('F'      ) : /* ACK: ignored                      */ break;
    case token_ctl('G'      ) : Q_EMIT bell();
                                break; //VT100
    case token_ctl('H'      ) : _currentScreen->backspace            (          ); break; //VT100
    case token_ctl('I'      ) : _currentScreen->tab                  (          ); break; //VT100
    case token_ctl('J'      ) : _currentScreen->newLine              (          ); break; //VT100
    case token_ctl('K'      ) : _currentScreen->newLine              (          ); break; //VT100
    case token_ctl('L'      ) : _currentScreen->newLine              (          ); break; //VT100
    case token_ctl('M'      ) : _currentScreen->toStartOfLine        (          ); break; //VT100

    case token_ctl('N'      ) :      useCharset           (         1); break; //VT100
    case token_ctl('O'      ) :      useCharset           (         0); break; //VT100

    case token_ctl('P'      ) : /* DLE: ignored                      */ break;
    case token_ctl('Q'      ) : /* DC1: XON continue                 */ break; //VT100
    case token_ctl('R'      ) : /* DC2: ignored                      */ break;
    case token_ctl('S'      ) : /* DC3: XOFF halt                    */ break; //VT100
    case token_ctl('T'      ) : /* DC4: ignored                      */ break;
    case token_ctl('U'      ) : /* NAK: ignored                      */ break;
    case token_ctl('V'      ) : /* SYN: ignored                      */ break;
    case token_ctl('W'      ) : /* ETB: ignored                      */ break;
    case token_ctl('X'      ) : _currentScreen->displayCharacter     (    0x2592); break; //VT100
    case token_ctl('Y'      ) : /* EM : ignored                      */ break;
    case token_ctl('Z'      ) : _currentScreen->displayCharacter     (    0x2592); break; //VT100
    case token_ctl('['      ) : /* ESC: cannot be seen here.         */ break;
    case token_ctl('\\'     ) : /* FS : ignored                      */ break;
    case token_ctl(']'      ) : /* GS : ignored                      */ break;
    case token_ctl('^'      ) : /* RS : ignored                      */ break;
    case token_ctl('_'      ) : /* US : ignored                      */ break;

    case token_esc('D'      ) : _currentScreen->index                (          ); break; //VT100
    case token_esc('E'      ) : _currentScreen->nextLine             (          ); break; //VT100
    case token_esc('H'      ) : _currentScreen->changeTabStop        (true      ); break; //VT100
    case token_esc('M'      ) : _currentScreen->reverseIndex         (          ); break; //VT100
    case token_esc('Z'      ) :      reportTerminalType   (          ); break;
    case token_esc('c'      ) :      reset                (          ); break;

    case token_esc('n'      ) :      useCharset           (         2); break;
    case token_esc('o'      ) :      useCharset           (         3); break;
    case token_esc('7'      ) :      saveCursor           (          ); break;
    case token_esc('8'      ) :      restoreCursor        (          ); break;

    case token_esc('='      ) :          setMode      (MODE_AppKeyPad); break;
    case token_esc('>'      ) :        resetMode      (MODE_AppKeyPad); break;
    case token_esc('<'      ) :          setMode      (MODE_Ansi     ); break; //VT100

    case token_esc('\\'      ) :       resetMode      (MODE_Sixel    ); break;

    case token_esc_cs('(', '0') :      setCharset           (0,    '0'); break; //VT100
    case token_esc_cs('(', 'A') :      setCharset           (0,    'A'); break; //VT100
    case token_esc_cs('(', 'B') :      setCharset           (0,    'B'); break; //VT100

    case token_esc_cs(')', '0') :      setCharset           (1,    '0'); break; //VT100
    case token_esc_cs(')', 'A') :      setCharset           (1,    'A'); break; //VT100
    case token_esc_cs(')', 'B') :      setCharset           (1,    'B'); break; //VT100

    case token_esc_cs('*', '0') :      setCharset           (2,    '0'); break; //VT100
    case token_esc_cs('*', 'A') :      setCharset           (2,    'A'); break; //VT100
    case token_esc_cs('*', 'B') :      setCharset           (2,    'B'); break; //VT100

    case token_esc_cs('+', '0') :      setCharset           (3,    '0'); break; //VT100
    case token_esc_cs('+', 'A') :      setCharset           (3,    'A'); break; //VT100
    case token_esc_cs('+', 'B') :      setCharset           (3,    'B'); break; //VT100

    case token_esc_cs('%', 'G') :      setCodec             (Utf8Codec   ); break; //LINUX
    case token_esc_cs('%', '@') :      setCodec             (LocaleCodec ); break; //LINUX

    case token_esc_de('3'      ) : /* Double height line, top half    */
                                _currentScreen->setLineProperty( LINE_DOUBLEWIDTH , true );
                                _currentScreen->setLineProperty( LINE_DOUBLEHEIGHT_TOP , true );
                                _currentScreen->setLineProperty( LINE_DOUBLEHEIGHT_BOTTOM , false );
                                    break;
    case token_esc_de('4'      ) : /* Double height line, bottom half */
                                _currentScreen->setLineProperty( LINE_DOUBLEWIDTH , true );
                                _currentScreen->setLineProperty( LINE_DOUBLEHEIGHT_TOP , false );
                                _currentScreen->setLineProperty( LINE_DOUBLEHEIGHT_BOTTOM , true );
                                    break;
    case token_esc_de('5'      ) : /* Single width, single height line*/
                                _currentScreen->setLineProperty( LINE_DOUBLEWIDTH , false);
                                _currentScreen->setLineProperty( LINE_DOUBLEHEIGHT_TOP , false);
                                _currentScreen->setLineProperty( LINE_DOUBLEHEIGHT_BOTTOM , false);
                                break;
    case token_esc_de('6'      ) : /* Double width, single height line*/
                                _currentScreen->setLineProperty( LINE_DOUBLEWIDTH , true);
                                _currentScreen->setLineProperty( LINE_DOUBLEHEIGHT_TOP , false);
                                _currentScreen->setLineProperty( LINE_DOUBLEHEIGHT_BOTTOM , false);
                                break;
    case token_esc_de('8'      ) : _currentScreen->helpAlign            (          ); break;

    // resize = \e[8;<rows>;<cols>t
    case token_csi_ps('t',   8) : setImageSize( p /* rows */, q /* columns */ );
                               Q_EMIT imageResizeRequest(QSize(q, p)); // Note columns (x), rows (y) in QSize
                               break;

    case token_csi_ps('t',   14) : reportPixelSize();          break;
    case token_csi_ps('t',   16) : reportCellSize();           break;
    case token_csi_ps('t',   18) : reportSize();               break;
    // change tab text color : \e[28;<color>t  color: 0-16,777,215
    case token_csi_ps('t',   28) : /* IGNORED: konsole-specific KDE3-era extension, not implemented */ break;

    case token_csi_ps('t',  22) : /* IGNORED: Save icon and window title on stack */      break; //XTERM
    case token_csi_ps('t',  23) : /* IGNORED: Restore icon and window title from stack */ break; //XTERM

    case token_csi_ps('K',   0) : _currentScreen->clearToEndOfLine     (          ); break;
    case token_csi_ps('K',   1) : _currentScreen->clearToBeginOfLine   (          ); break;
    case token_csi_ps('K',   2) : _currentScreen->clearEntireLine      (          ); break;
    case token_csi_ps('J',   0) : _currentScreen->clearToEndOfScreen   (          ); break;
    case token_csi_ps('J',   1) : _currentScreen->clearToBeginOfScreen (          ); break;
    case token_csi_ps('J',   2) : _currentScreen->clearEntireScreen    (          ); break;
    case token_csi_ps('J',      3) : clearHistory();                            break;
    case token_csi_ps('g',   0) : _currentScreen->changeTabStop        (false     ); break; //VT100
    case token_csi_ps('g',   3) : _currentScreen->clearTabStops        (          ); break; //VT100
    case token_csi_ps('h',   4) : _currentScreen->    setMode      (MODE_Insert   ); break;
    case token_csi_ps('h',  20) :          setMode      (MODE_NewLine  ); break;
    case token_csi_ps('i',   0) : /* IGNORE: attached printer          */ break; //VT100
    case token_csi_ps('l',   4) : _currentScreen->  resetMode      (MODE_Insert   ); break;
    case token_csi_ps('l',  20) :        resetMode      (MODE_NewLine  ); break;
    case token_csi_ps('s',   0) :      saveCursor           (          ); break;
    case token_csi_ps('u',   0) :      restoreCursor        (          ); break;

    case token_csi_ps('m',   0) : _currentScreen->setDefaultRendition  (          ); break;
    case token_csi_ps('m',   1) : _currentScreen->  setRendition     (RE_BOLD     ); break; //VT100
    case token_csi_ps('m',   2) : _currentScreen->  setRendition     (RE_FAINT    ); break;
    case token_csi_ps('m',   3) : _currentScreen->  setRendition     (RE_ITALIC   ); break; //VT100
    case token_csi_ps('m',   4) : _currentScreen->  setRendition     (RE_UNDERLINE); break; //VT100
    case token_csi_ps('m',   5) : _currentScreen->  setRendition     (RE_BLINK    ); break; //VT100
    case token_csi_ps('m',   7) : _currentScreen->  setRendition     (RE_REVERSE  ); break;
    case token_csi_ps('m',   8) : _currentScreen->  setRendition     (RE_CONCEAL  ); break;
    case token_csi_ps('m',   9) : _currentScreen->  setRendition     (RE_STRIKEOUT); break;
    case token_csi_ps('m',  53) : _currentScreen->  setRendition     (RE_OVERLINE ); break;
    case token_csi_ps('m',  10) : /* IGNORED: mapping related          */ break; //LINUX
    case token_csi_ps('m',  11) : /* IGNORED: mapping related          */ break; //LINUX
    case token_csi_ps('m',  12) : /* IGNORED: mapping related          */ break; //LINUX
    case token_csi_ps('m',  21) : _currentScreen->resetRendition     (RE_BOLD     ); break;
    case token_csi_ps('m',  22) : _currentScreen->resetRendition     (RE_BOLD     );
                               _currentScreen->resetRendition     (RE_FAINT    ); break;
    case token_csi_ps('m',  23) : _currentScreen->resetRendition     (RE_ITALIC   ); break; //VT100
    case token_csi_ps('m',  24) : _currentScreen->resetRendition     (RE_UNDERLINE); break;
    case token_csi_ps('m',  25) : _currentScreen->resetRendition     (RE_BLINK    ); break;
    case token_csi_ps('m',  27) : _currentScreen->resetRendition     (RE_REVERSE  ); break;
    case token_csi_ps('m',  28) : _currentScreen->resetRendition     (RE_CONCEAL  ); break;
    case token_csi_ps('m',  29) : _currentScreen->resetRendition     (RE_STRIKEOUT); break;
    case token_csi_ps('m',  55) : _currentScreen->resetRendition     (RE_OVERLINE ); break;

    case token_csi_ps('m',   30) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  0); break;
    case token_csi_ps('m',   31) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  1); break;
    case token_csi_ps('m',   32) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  2); break;
    case token_csi_ps('m',   33) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  3); break;
    case token_csi_ps('m',   34) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  4); break;
    case token_csi_ps('m',   35) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  5); break;
    case token_csi_ps('m',   36) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  6); break;
    case token_csi_ps('m',   37) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  7); break;

    case token_csi_ps('m',   38) : _currentScreen->setForeColor         (p,       q); break;

    case token_csi_ps('m',   39) : _currentScreen->setForeColor         (COLOR_SPACE_DEFAULT,  0); break;

    case token_csi_ps('m',   40) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  0); break;
    case token_csi_ps('m',   41) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  1); break;
    case token_csi_ps('m',   42) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  2); break;
    case token_csi_ps('m',   43) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  3); break;
    case token_csi_ps('m',   44) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  4); break;
    case token_csi_ps('m',   45) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  5); break;
    case token_csi_ps('m',   46) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  6); break;
    case token_csi_ps('m',   47) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  7); break;

    case token_csi_ps('m',   48) : _currentScreen->setBackColor         (p,       q); break;

    case token_csi_ps('m',   49) : _currentScreen->setBackColor         (COLOR_SPACE_DEFAULT,  1); break;

    case token_csi_ps('m',   90) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  8); break;
    case token_csi_ps('m',   91) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  9); break;
    case token_csi_ps('m',   92) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 10); break;
    case token_csi_ps('m',   93) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 11); break;
    case token_csi_ps('m',   94) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 12); break;
    case token_csi_ps('m',   95) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 13); break;
    case token_csi_ps('m',   96) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 14); break;
    case token_csi_ps('m',   97) : _currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 15); break;

    case token_csi_ps('m',  100) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  8); break;
    case token_csi_ps('m',  101) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  9); break;
    case token_csi_ps('m',  102) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 10); break;
    case token_csi_ps('m',  103) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 11); break;
    case token_csi_ps('m',  104) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 12); break;
    case token_csi_ps('m',  105) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 13); break;
    case token_csi_ps('m',  106) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 14); break;
    case token_csi_ps('m',  107) : _currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 15); break;

    case token_csi_ps('n',   5) :      reportStatus         (          ); break;
    case token_csi_ps('n',   6) :      reportCursorPosition (          ); break;
    case token_csi_ps('q',   0) : /* IGNORED: LEDs off                 */ break; //VT100
    case token_csi_ps('q',   1) : /* IGNORED: LED1 on                  */ break; //VT100
    case token_csi_ps('q',   2) : /* IGNORED: LED2 on                  */ break; //VT100
    case token_csi_ps('q',   3) : /* IGNORED: LED3 on                  */ break; //VT100
    case token_csi_ps('q',   4) : /* IGNORED: LED4 on                  */ break; //VT100
    case token_csi_ps('x',   0) :      reportTerminalParms  (         2); break; //VT100
    case token_csi_ps('x',   1) :      reportTerminalParms  (         3); break; //VT100

    case token_csi_pn('@'      ) : _currentScreen->insertChars          (p         ); break;
    case token_csi_pn('A'      ) : _currentScreen->cursorUp             (p         ); break; //VT100
    case token_csi_pn('B'      ) : _currentScreen->cursorDown           (p         ); break; //VT100
    case token_csi_pn('C'      ) : _currentScreen->cursorRight          (p         ); break; //VT100
    case token_csi_pn('D'      ) : _currentScreen->cursorLeft           (p         ); break; //VT100
    case token_csi_pn('E'      ) : _currentScreen->cursorNextLine       (p         ); break; //VT100
    case token_csi_pn('F'      ) : _currentScreen->cursorPreviousLine   (p         ); break; //VT100
    case token_csi_pn('G'      ) : _currentScreen->setCursorX           (p         ); break; //LINUX
    case token_csi_pn('H'      ) : _currentScreen->setCursorYX          (p,      q); break; //VT100
    case token_csi_pn('I'      ) : _currentScreen->tab                  (p         ); break;
    case token_csi_pn('L'      ) : _currentScreen->insertLines          (p         ); break;
    case token_csi_pn('M'      ) : _currentScreen->deleteLines          (p         ); break;
    case token_csi_pn('P'      ) : _currentScreen->deleteChars          (p         ); break;
    case token_csi_pn('S'      ) : _currentScreen->scrollUp             (p         ); break;
    case token_csi_pn('T'      ) : _currentScreen->scrollDown           (p         ); break;
    case token_csi_pn('X'      ) : _currentScreen->eraseChars           (p         ); break;
    case token_csi_pn('Z'      ) : _currentScreen->backtab              (p         ); break;
    case token_csi_pn('b'      ) : _currentScreen->repeatChars          (p         ); break;
    case token_csi_pn('c'      ) :      reportTerminalType   (          ); break; //VT100
    case token_csi_pn('d'      ) : _currentScreen->setCursorY           (p         ); break; //LINUX
    case token_csi_pn('f'      ) : _currentScreen->setCursorYX          (p,      q); break; //VT100
    case token_csi_pn('r'      ) :      setMargins           (p,      q); break; //VT100
    case token_csi_pn('y'      ) : /* IGNORED: Confidence test          */ break; //VT100

    case token_csi_pr('h',   1) :          setMode      (MODE_AppCuKeys); break; //VT100
    case token_csi_pr('l',   1) :        resetMode      (MODE_AppCuKeys); break; //VT100
    case token_csi_pr('s',   1) :         saveMode      (MODE_AppCuKeys); break; //FIXME
    case token_csi_pr('r',   1) :      restoreMode      (MODE_AppCuKeys); break; //FIXME

    case token_csi_pr('l',   2) :        resetMode      (MODE_Ansi     ); break; //VT100

    case token_csi_pr('h',   3) :          setMode      (MODE_132Columns); break; //VT100
    case token_csi_pr('l',   3) :        resetMode      (MODE_132Columns); break; //VT100

    case token_csi_pr('h',   4) : /* IGNORED: soft scrolling           */ break; //VT100
    case token_csi_pr('l',   4) : /* IGNORED: soft scrolling           */ break; //VT100

    case token_csi_pr('h',   5) : _currentScreen->    setMode      (MODE_Screen   ); break; //VT100
    case token_csi_pr('l',   5) : _currentScreen->  resetMode      (MODE_Screen   ); break; //VT100

    case token_csi_pr('h',   6) : _currentScreen->    setMode      (MODE_Origin   ); break; //VT100
    case token_csi_pr('l',   6) : _currentScreen->  resetMode      (MODE_Origin   ); break; //VT100
    case token_csi_pr('s',   6) : _currentScreen->   saveMode      (MODE_Origin   ); break; //FIXME
    case token_csi_pr('r',   6) : _currentScreen->restoreMode      (MODE_Origin   ); break; //FIXME

    case token_csi_pr('h',   7) : _currentScreen->    setMode      (MODE_Wrap     ); break; //VT100
    case token_csi_pr('l',   7) : _currentScreen->  resetMode      (MODE_Wrap     ); break; //VT100
    case token_csi_pr('s',   7) : _currentScreen->   saveMode      (MODE_Wrap     ); break; //FIXME
    case token_csi_pr('r',   7) : _currentScreen->restoreMode      (MODE_Wrap     ); break; //FIXME

    case token_csi_pr('h',   8) : /* IGNORED: autorepeat on            */ break; //VT100
    case token_csi_pr('l',   8) : /* IGNORED: autorepeat off           */ break; //VT100
    case token_csi_pr('s',   8) : /* IGNORED: autorepeat on            */ break; //VT100
    case token_csi_pr('r',   8) : /* IGNORED: autorepeat off           */ break; //VT100

    case token_csi_pr('h',   9) : /* IGNORED: interlace                */ break; //VT100
    case token_csi_pr('l',   9) : /* IGNORED: interlace                */ break; //VT100
    case token_csi_pr('s',   9) : /* IGNORED: interlace                */ break; //VT100
    case token_csi_pr('r',   9) : /* IGNORED: interlace                */ break; //VT100

    case token_csi_pr('h',  12) : /* IGNORED: Cursor blink             */ break; //att610
    case token_csi_pr('l',  12) : /* IGNORED: Cursor blink             */ break; //att610
    case token_csi_pr('s',  12) : /* IGNORED: Cursor blink             */ break; //att610
    case token_csi_pr('r',  12) : /* IGNORED: Cursor blink             */ break; //att610

    case token_csi_pr('h',  25) :          setMode      (MODE_Cursor   ); break; //VT100
    case token_csi_pr('l',  25) :        resetMode      (MODE_Cursor   ); break; //VT100
    case token_csi_pr('s',  25) :         saveMode      (MODE_Cursor   ); break; //VT100
    case token_csi_pr('r',  25) :      restoreMode      (MODE_Cursor   ); break; //VT100

    case token_csi_pr('h',  40) :         setMode(MODE_Allow132Columns ); break; // XTERM
    case token_csi_pr('l',  40) :       resetMode(MODE_Allow132Columns ); break; // XTERM

    case token_csi_pr('h',  41) : /* IGNORED: obsolete more(1) fix     */ break; //XTERM
    case token_csi_pr('l',  41) : /* IGNORED: obsolete more(1) fix     */ break; //XTERM
    case token_csi_pr('s',  41) : /* IGNORED: obsolete more(1) fix     */ break; //XTERM
    case token_csi_pr('r',  41) : /* IGNORED: obsolete more(1) fix     */ break; //XTERM

    case token_csi_pr('h',  47) :          setMode      (MODE_AppScreen); break; //VT100
    case token_csi_pr('l',  47) :        resetMode      (MODE_AppScreen); break; //VT100
    case token_csi_pr('s',  47) :         saveMode      (MODE_AppScreen); break; //XTERM
    case token_csi_pr('r',  47) :      restoreMode      (MODE_AppScreen); break; //XTERM

    case token_csi_pr('h',  67) : /* IGNORED: DECBKM                   */ break; //XTERM
    case token_csi_pr('l',  67) : /* IGNORED: DECBKM                   */ break; //XTERM
    case token_csi_pr('s',  67) : /* IGNORED: DECBKM                   */ break; //XTERM
    case token_csi_pr('r',  67) : /* IGNORED: DECBKM                   */ break; //XTERM

    case token_csi_pr('h',  80) : m_SixelScrolling = false;               break;
    case token_csi_pr('l',  80) : m_SixelScrolling = true;                break;

    // XTerm defines the following modes:
    // SET_VT200_MOUSE             1000
    // SET_VT200_HIGHLIGHT_MOUSE   1001
    // SET_BTN_EVENT_MOUSE         1002
    // SET_ANY_EVENT_MOUSE         1003
    //

    //Note about mouse modes:
    //There are four mouse modes which xterm-compatible terminals can support - 1000,1001,1002,1003
    //Konsole currently supports mode 1000 (basic mouse press and release),  mode 1002 (dragging the mouse)
    //and mode 1003 (moving the mouse).
    //TODO:  Implementation of mouse mode 1001 (something called highlight tracking).
    //

    case token_csi_pr('h', 1000) :          setMode      (MODE_Mouse1000); break; //XTERM
    case token_csi_pr('l', 1000) :        resetMode      (MODE_Mouse1000); break; //XTERM
    case token_csi_pr('s', 1000) :         saveMode      (MODE_Mouse1000); break; //XTERM
    case token_csi_pr('r', 1000) :      restoreMode      (MODE_Mouse1000); break; //XTERM

    case token_csi_pr('h', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM
    case token_csi_pr('l', 1001) :        resetMode      (MODE_Mouse1001); break; //XTERM
    case token_csi_pr('s', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM
    case token_csi_pr('r', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM

    case token_csi_pr('h', 1002) :          setMode      (MODE_Mouse1002); break; //XTERM
    case token_csi_pr('l', 1002) :        resetMode      (MODE_Mouse1002); break; //XTERM
    case token_csi_pr('s', 1002) :         saveMode      (MODE_Mouse1002); break; //XTERM
    case token_csi_pr('r', 1002) :      restoreMode      (MODE_Mouse1002); break; //XTERM

    case token_csi_pr('h', 1003) :          setMode      (MODE_Mouse1003); break; //XTERM
    case token_csi_pr('l', 1003) :        resetMode      (MODE_Mouse1003); break; //XTERM
    case token_csi_pr('s', 1003) :         saveMode      (MODE_Mouse1003); break; //XTERM
    case token_csi_pr('r', 1003) :      restoreMode      (MODE_Mouse1003); break; //XTERM

    case token_csi_pr('h',  1004) : _reportFocusEvents = true; break;
    case token_csi_pr('l',  1004) : _reportFocusEvents = false; break;

    case token_csi_pr('h', 1005) :          setMode      (MODE_Mouse1005); break; //XTERM
    case token_csi_pr('l', 1005) :        resetMode      (MODE_Mouse1005); break; //XTERM
    case token_csi_pr('s', 1005) :         saveMode      (MODE_Mouse1005); break; //XTERM
    case token_csi_pr('r', 1005) :      restoreMode      (MODE_Mouse1005); break; //XTERM

    case token_csi_pr('h', 1006) :          setMode      (MODE_Mouse1006); break; //XTERM
    case token_csi_pr('l', 1006) :        resetMode      (MODE_Mouse1006); break; //XTERM
    case token_csi_pr('s', 1006) :         saveMode      (MODE_Mouse1006); break; //XTERM
    case token_csi_pr('r', 1006) :      restoreMode      (MODE_Mouse1006); break; //XTERM

    case token_csi_pr('h', 1007) :          setMode      (MODE_Mouse1007); break; //XTERM
    case token_csi_pr('l', 1007) :        resetMode      (MODE_Mouse1007); break; //XTERM
    case token_csi_pr('s', 1007) :         saveMode      (MODE_Mouse1007); break; //XTERM
    case token_csi_pr('r', 1007) :      restoreMode      (MODE_Mouse1007); break; //XTERM

    case token_csi_pr('h', 1015) :          setMode      (MODE_Mouse1015); break; //URXVT
    case token_csi_pr('l', 1015) :        resetMode      (MODE_Mouse1015); break; //URXVT
    case token_csi_pr('s', 1015) :         saveMode      (MODE_Mouse1015); break; //URXVT
    case token_csi_pr('r', 1015) :      restoreMode      (MODE_Mouse1015); break; //URXVT

    case token_csi_pr('h', 1034) : /* IGNORED: 8bitinput activation     */ break; //XTERM

    case token_csi_pr('h', 1047) :          setMode      (MODE_AppScreen); break; //XTERM
    case token_csi_pr('l', 1047) : _screen[1]->clearEntireScreen(); resetMode(MODE_AppScreen); break; //XTERM
    case token_csi_pr('s', 1047) :         saveMode      (MODE_AppScreen); break; //XTERM
    case token_csi_pr('r', 1047) :      restoreMode      (MODE_AppScreen); break; //XTERM

    //FIXME: Unitoken: save translations
    case token_csi_pr('h', 1048) :      saveCursor           (          ); break; //XTERM
    case token_csi_pr('l', 1048) :      restoreCursor        (          ); break; //XTERM
    case token_csi_pr('s', 1048) :      saveCursor           (          ); break; //XTERM
    case token_csi_pr('r', 1048) :      restoreCursor        (          ); break; //XTERM

    //FIXME: every once new sequences like this pop up in xterm.
    //       Here's a guess of what they could mean.
    case token_csi_pr('h', 1049) : saveCursor(); _screen[1]->clearEntireScreen(); setMode(MODE_AppScreen); break; //XTERM
    case token_csi_pr('l', 1049) : resetMode(MODE_AppScreen); restoreCursor(); break; //XTERM

    case token_csi_pr('h', 2004) :          setMode      (MODE_BracketedPaste); break; //XTERM
    case token_csi_pr('l', 2004) :        resetMode      (MODE_BracketedPaste); break; //XTERM
    case token_csi_pr('s', 2004) :         saveMode      (MODE_BracketedPaste); break; //XTERM
    case token_csi_pr('r', 2004) :      restoreMode      (MODE_BracketedPaste); break; //XTERM

    case token_csi_pr('S',    1) : if(!p) sixelQuery        (1          ); break;
    case token_csi_pr('S',    2) : if(!p) sixelQuery        (2          ); break;
    // Set Cursor Style (DECSCUSR), VT520, with the extra xterm sequences
    // the first one is a special case, 'ESC[ q', which mimics 'ESC[1 q'
    // Using 0 to reset to default is matching VTE, but not any official standard.
    case token_csi_sp ('q'    ) : Q_EMIT setCursorStyleRequest(Enum::BlockCursor,     true);  break;
    case token_csi_psp('q',  0) : Q_EMIT resetCursorStyleRequest();                           break;
    case token_csi_psp('q',  1) : Q_EMIT setCursorStyleRequest(Enum::BlockCursor,     true);  break;
    case token_csi_psp('q',  2) : Q_EMIT setCursorStyleRequest(Enum::BlockCursor,     false); break;
    case token_csi_psp('q',  3) : Q_EMIT setCursorStyleRequest(Enum::UnderlineCursor, true);  break;
    case token_csi_psp('q',  4) : Q_EMIT setCursorStyleRequest(Enum::UnderlineCursor, false); break;
    case token_csi_psp('q',  5) : Q_EMIT setCursorStyleRequest(Enum::IBeamCursor,     true);  break;
    case token_csi_psp('q',  6) : Q_EMIT setCursorStyleRequest(Enum::IBeamCursor,     false); break;

    // DECSTR (Soft Terminal Reset)
    case token_csi_pe('p'      ) : reset(true); break; //VT220

    case token_csi_pq('c'      ) :  reportTertiaryAttributes(          ); break; //VT420
    case token_csi_pg('c'      ) :  reportSecondaryAttributes(          ); break; //VT100
    case token_csi_pg('q'      ) :  reportVersion(          ); break;

    //FIXME: when changing between vt52 and ansi mode evtl do some resetting.
    case token_vt52('A'      ) : _currentScreen->cursorUp             (         1); break; //VT52
    case token_vt52('B'      ) : _currentScreen->cursorDown           (         1); break; //VT52
    case token_vt52('C'      ) : _currentScreen->cursorRight          (         1); break; //VT52
    case token_vt52('D'      ) : _currentScreen->cursorLeft           (         1); break; //VT52

    case token_vt52('F'      ) :      setAndUseCharset     (0,    '0'); break; //VT52
    case token_vt52('G'      ) :      setAndUseCharset     (0,    'B'); break; //VT52

    case token_vt52('H'      ) : _currentScreen->setCursorYX          (1,1       ); break; //VT52
    case token_vt52('I'      ) : _currentScreen->reverseIndex         (          ); break; //VT52
    case token_vt52('J'      ) : _currentScreen->clearToEndOfScreen   (          ); break; //VT52
    case token_vt52('K'      ) : _currentScreen->clearToEndOfLine     (          ); break; //VT52
    case token_vt52('Y'      ) : _currentScreen->setCursorYX          (p-31,q-31 ); break; //VT52
    case token_vt52('Z'      ) :      reportTerminalType   (           ); break; //VT52
    case token_vt52('<'      ) :          setMode      (MODE_Ansi     ); break; //VT52
    case token_vt52('='      ) :          setMode      (MODE_AppKeyPad); break; //VT52
    case token_vt52('>'      ) :        resetMode      (MODE_AppKeyPad); break; //VT52

    default:
        reportDecodingError(token);
        break;
    }
    /* clang-format on */
}

void delete_func(void *p)
{
    delete (QByteArray *)p;
}

void Vt102Emulation::processGraphicsToken(int tokenSize)
{
    QString value = QString::fromUcs4(&tokenBuffer[1], tokenSize - 1);
    QStringList list;
    QPixmap pixmap;

    int dataPos = value.indexOf(QLatin1Char(';'));
    if (dataPos == -1) {
        dataPos = value.size();
    }
    if (dataPos > 1024) {
        reportDecodingError(token_apc('G'));
        return;
    }
    list = value.mid(0, dataPos).split(QLatin1Char(','));

    QMap<char, qint64> keys; // Keys may be signed or unsigned 32 bit integers

    if (savedKeys.empty()) {
        keys['a'] = 't';
        keys['t'] = 'd';
        keys['q'] = 0;
        keys['m'] = 0;
        keys['f'] = 32;
        keys['i'] = 0;
        keys['o'] = 0;
        keys['X'] = 0;
        keys['Y'] = 0;
        keys['x'] = 0;
        keys['y'] = 0;
        keys['z'] = 0;
        keys['C'] = 0;
        keys['c'] = 0;
        keys['r'] = 0;
        keys['A'] = 255;
        keys['I'] = 0;
        keys['d'] = 'a';
        keys['p'] = -1;
    } else {
        keys = QMap<char, qint64>(savedKeys);
    }

    for (int i = 0; i < list.size(); i++) {
        if (list.at(i).at(1).toLatin1() != '=') {
            reportDecodingError(token_apc('G'));
            return;
        }
        if (list.at(i).at(2).isNumber() || list.at(i).at(2).toLatin1() == '-') {
            keys[list.at(i).at(0).toLatin1()] = QStringView(list.at(i)).mid(2).toInt();
        } else {
            keys[list.at(i).at(0).toLatin1()] = list.at(i).at(2).toLatin1();
        }
    }

    if (keys['a'] == 't' || keys['a'] == 'T' || keys['a'] == 'q') {
        if (keys['q'] < 2 && keys['t'] != 'd') {
            QString params = QStringLiteral("i=") + QString::number(keys['i']);
            QString error = QStringLiteral("ENOTSUPPORTED:");
            sendGraphicsReply(params, error);
            return;
        }
        if (keys['I']) {
            keys['i'] = getFreeGraphicsImageId();
        }
        if (imageId != keys['i']) {
            imageId = keys['i'];
            imageData.clear();
        }
        imageData.append(tokenData);
        tokenData.clear();
        imageData.append(QByteArray::fromBase64(value.mid(dataPos + 1).toLocal8Bit()));
        if (keys['m'] == 0) {
            imageId = 0;
            savedKeys = QMap<char, qint64>();
            QByteArray out;

            uint32_t byteCount = 0;
            if (keys['f'] == 24 || keys['f'] == 32) {
                int bpp = keys['f'] / 8;
                byteCount = bpp * keys['s'] * keys['v'];
            } else {
                byteCount = 8 * 1024 * 1024;
            }

            if (keys['o'] == 'z') {
                char header[sizeof byteCount];
                qToBigEndian(byteCount, header);
                imageData.prepend(header, sizeof header);
                out = qUncompress(imageData);

                if (keys['f'] != 24 && keys['f'] != 32) {
                    imageData = out;
                }
            }
            if (out.isEmpty()) {
                out = imageData;
            }

            if (keys['f'] == 24 || keys['f'] == 32) {
                if (unsigned(out.size()) < byteCount) {
                    qCWarning(KonsoleDebug) << "Not enough image data" << out.size() << "require" << byteCount;
                    imageData.clear();
                    return;
                }
                QImage::Format format = keys['f'] == 24 ? QImage::Format_RGB888 : QImage::Format_RGBA8888;
                pixmap = QPixmap::fromImage(QImage((const uchar *)out.constData(), 0 + keys['s'], 0 + keys['v'], 0 + keys['s'] * keys['f'] / 8, format));
                pixmap.detach();
            } else {
                pixmap.loadFromData(out);
            }

            if (keys['a'] == 'q') {
                QString params = QStringLiteral("i=") + QString::number(keys['i']);
                sendGraphicsReply(params, QString());
            } else {
                if (keys['i']) {
                    _graphicsImages[keys['i']] = pixmap;
                }
                if (keys['q'] == 0 && keys['a'] == 't') {
                    QString params = QStringLiteral("i=") + QString::number(keys['i']);
                    if (keys['I']) {
                        params = params + QStringLiteral(",I=") + QString::number(keys['I']);
                    }
                    sendGraphicsReply(params, QString());
                }
            }
            imageData.clear();
        } else {
            if (savedKeys.empty()) {
                savedKeys = QMap<char, qint64>(keys);
                savedKeys.remove('m');
            }
        }
    }
    if (keys['a'] == 'p' || (keys['a'] == 'T' && keys['m'] == 0)) {
        if (keys['a'] == 'p') {
            pixmap = _graphicsImages[keys['i']];
        }
        if (!pixmap.isNull()) {
            if (keys['x'] || keys['y'] || keys['w'] || keys['h']) {
                int w = keys['w'] ? keys['w'] : pixmap.width() - keys['x'];
                int h = keys['h'] ? keys['h'] : pixmap.height() - keys['y'];
                pixmap = pixmap.copy(keys['x'], keys['y'], w, h);
            }
            if (keys['c'] && keys['r']) {
                pixmap = pixmap.scaled(keys['c'] * _currentScreen->currentTerminalDisplay()->terminalFont()->fontWidth(),
                                       keys['r'] * _currentScreen->currentTerminalDisplay()->terminalFont()->fontHeight());
            }
            int rows = -1, cols = -1;
            _currentScreen->addPlacement(pixmap,
                                         rows,
                                         cols,
                                         -1,
                                         -1,
                                         true,
                                         keys['C'] == 0,
                                         true,
                                         keys['z'],
                                         keys['i'],
                                         keys['p'],
                                         keys['A'] / 255.0,
                                         keys['X'],
                                         keys['Y']);
            if (keys['q'] == 0 && keys['i']) {
                QString params = QStringLiteral("i=") + QString::number(keys['i']);
                if (keys['I']) {
                    params = params + QStringLiteral(",I=") + QString::number(keys['I']);
                }
                if (keys['p'] >= 0) {
                    params = params + QStringLiteral(",p=") + QString::number(keys['p']);
                }
                sendGraphicsReply(params, QString());
            }
        } else {
            if (keys['q'] < 2) {
                QString params = QStringLiteral("i=") + QString::number(keys['i']);
                sendGraphicsReply(params, QStringLiteral("ENOENT:No such image"));
            }
        }
    }
    if (keys['a'] == 'd') {
        int action = keys['d'] | 0x20;
        int id = keys['i'];
        int pid = keys['p'];
        int x = keys['x'];
        int y = keys['y'];
        if (action == 'n') {
        } else if (action == 'c') {
            action = 'p';
            x = _currentScreen->getCursorX();
            y = _currentScreen->getCursorY();
        }
        _currentScreen->delPlacements(action, id, pid, x, y, keys['z']);
    }
}

void Vt102Emulation::clearScreenAndSetColumns(int columnCount)
{
    setImageSize(_currentScreen->getLines(), columnCount);
    clearEntireScreen();
    setDefaultMargins();
    _currentScreen->setCursorYX(0, 0);
}

void Vt102Emulation::sendString(const QByteArray &s)
{
    Q_EMIT sendData(s);
}

void Vt102Emulation::sendGraphicsReply(const QString &params, const QString &error)
{
    sendString(
        (QStringLiteral("\033_G") + params + QStringLiteral(";") + (error.isEmpty() ? QStringLiteral("OK") : error) + QStringLiteral("\033\\")).toLatin1());
}

void Vt102Emulation::reportCursorPosition()
{
    char tmp[30];
    int y = _currentScreen->getCursorY() + 1;
    int x = _currentScreen->getCursorX() + 1;
    if (_currentScreen->getMode(MODE_Origin)) {
        y -= _currentScreen->topMargin();
    }
    snprintf(tmp, sizeof(tmp), "\033[%d;%dR", y, x);
    sendString(tmp);
}

void Vt102Emulation::reportPixelSize()
{
    char tmp[30];
    snprintf(tmp,
             sizeof(tmp),
             "\033[4;%d;%dt",
             _currentScreen->currentTerminalDisplay()->terminalFont()->fontHeight() * _currentScreen->getLines(),
             _currentScreen->currentTerminalDisplay()->terminalFont()->fontWidth() * _currentScreen->getColumns());
    sendString(tmp);
}

void Vt102Emulation::iTermReportCellSize()
{
    char tmp[50];
    snprintf(tmp,
             sizeof(tmp),
             "\033]1337;ReportCellSize=%d.0;%d.0;1.0\007",
             _currentScreen->currentTerminalDisplay()->terminalFont()->fontHeight(),
             _currentScreen->currentTerminalDisplay()->terminalFont()->fontWidth());
    sendString(tmp);
}

void Vt102Emulation::reportCellSize()
{
    char tmp[30];
    snprintf(tmp,
             sizeof(tmp),
             "\033[6;%d;%dt",
             _currentScreen->currentTerminalDisplay()->terminalFont()->fontHeight(),
             _currentScreen->currentTerminalDisplay()->terminalFont()->fontWidth());
    sendString(tmp);
}

void Vt102Emulation::reportColor(int c, QColor color)
{
    char tmp[60];
    snprintf(tmp,
             sizeof(tmp),
             "\033]4;%i;rgb:%02x%02x/%02x%02x/%02x%02x\007",
             c,
             color.red(),
             color.red(),
             color.green(),
             color.green(),
             color.blue(),
             color.blue());
    sendString(tmp);
}

void Vt102Emulation::reportSize()
{
    char tmp[30];
    snprintf(tmp, sizeof(tmp), "\033[8;%d;%dt", _currentScreen->getLines(), _currentScreen->getColumns());
    sendString(tmp);
}

void Vt102Emulation::reportTerminalType()
{
    // Primary device attribute response (Request was: ^[[0c or ^[[c (from TT321 Users Guide))
    // VT220:  ^[[?63;1;2;3;6;7;8c   (list deps on emul. capabilities)
    // VT100:  ^[[?1;2c
    // VT101:  ^[[?1;0c
    // VT102:  ^[[?6v
    if (getMode(MODE_Ansi)) {
        sendString("\033[?62;1;4c"); // I'm a VT2xx with 132 columns and Sixel
    } else {
        sendString("\033/Z"); // I'm a VT52
    }
}

void Vt102Emulation::reportTertiaryAttributes()
{
    // Tertiary device attribute response DECRPTUI (Request was: ^[[=0c or ^[[=c)
    // 7E4B4445 is hex for ASCII "~KDE"
    sendString("\033P!|7E4B4445\033\\");
}

void Vt102Emulation::reportSecondaryAttributes()
{
    // Secondary device attribute response (Request was: ^[[>0c or ^[[>c)
    if (getMode(MODE_Ansi)) {
        sendString("\033[>1;115;0c"); // Why 115?  ;)
    } else {
        sendString("\033/Z"); // FIXME I don't think VT52 knows about it but kept for
    }
    // konsoles backward compatibility.
}

void Vt102Emulation::reportVersion()
{
    sendString("\033P>|Konsole " KONSOLE_VERSION "\033\\");
}

/* DECREPTPARM – Report Terminal Parameters
    ESC [ <sol>; <par>; <nbits>; <xspeed>; <rspeed>; <clkmul>; <flags> x

    https://vt100.net/docs/vt100-ug/chapter3.html
*/
void Vt102Emulation::reportTerminalParms(int p)
{
    char tmp[100];
    /*
       sol=1: This message is a request; report in response to a request.
       par=1: No parity set
       nbits=1: 8 bits per character
       xspeed=112: 9600
       rspeed=112: 9600
       clkmul=1: The bit rate multiplier is 16.
       flags=0: None
    */
    snprintf(tmp, sizeof(tmp), "\033[%d;1;1;112;112;1;0x", p); // not really true.
    sendString(tmp);
}

void Vt102Emulation::reportStatus()
{
    sendString("\033[0n"); // VT100. Device status report. 0 = Ready.
}

void Vt102Emulation::reportAnswerBack()
{
    // FIXME - Test this with VTTEST
    // This is really obsolete VT100 stuff.
    const char *ANSWER_BACK = "";
    sendString(ANSWER_BACK);
}

/*!
    `cx',`cy' are 1-based.
    `cb' indicates the button pressed or released (0-2) or scroll event (4-5).

    eventType represents the kind of mouse action that occurred:
        0 = Mouse button press
        1 = Mouse drag
        2 = Mouse button release
        3 = Mouse click to move cursor in input field
*/

void Vt102Emulation::sendMouseEvent(int cb, int cx, int cy, int eventType)
{
    if (cx < 1 || cy < 1) {
        return;
    }

    if (eventType == 3) {
        // We know we are in input mode
        TerminalDisplay *currentView = _currentScreen->currentTerminalDisplay();
        bool isReadOnly = false;
        if (currentView != nullptr && currentView->sessionController() != nullptr) {
            isReadOnly = currentView->sessionController()->isReadOnly();
        }
        auto point = std::make_pair(cy, cx);
        if (!isReadOnly && _currentScreen->replModeStart() <= point && point <= _currentScreen->replModeEnd()) {
            KeyboardTranslator::States states = KeyboardTranslator::NoState;

            // get current states
            if (getMode(MODE_NewLine)) {
                states |= KeyboardTranslator::NewLineState;
            }
            if (getMode(MODE_Ansi)) {
                states |= KeyboardTranslator::AnsiState;
            }
            if (getMode(MODE_AppCuKeys)) {
                states |= KeyboardTranslator::CursorKeysState;
            }
            if (getMode(MODE_AppScreen)) {
                states |= KeyboardTranslator::AlternateScreenState;
            }
            KeyboardTranslator::Entry LRKeys[2] = {_keyTranslator->findEntry(Qt::Key_Left, Qt::NoModifier, states),
                                                   _keyTranslator->findEntry(Qt::Key_Right, Qt::NoModifier, states)};
            QVector<LineProperty> lineProperties = _currentScreen->getLineProperties(cy + _currentScreen->getHistLines(), cy + _currentScreen->getHistLines());
            cx = qMin(cx, LineLength(lineProperties[0]));
            int cuX = _currentScreen->getCursorX();
            int cuY = _currentScreen->getCursorY();
            QByteArray textToSend;
            if (cuY != cy) {
                for (int i = abs(cy - cuY); i > 0; i--) {
                    emulateUpDown(cy < cuY, LRKeys[cy > cuY], textToSend, i == 1 ? cx : -1);
                    textToSend += LRKeys[cy > cuY].text();
                }
            } else {
                if (cuX < cx) {
                    for (int i = 0; i < cx - cuX; i++) {
                        textToSend += LRKeys[1].text();
                    }
                } else {
                    for (int i = 0; i < cuX - cx; i++) {
                        textToSend += LRKeys[0].text();
                    }
                }
            }
            Q_EMIT sendData(textToSend);
        }
        return;
    }

    // Don't send move/drag events if only press and release requested
    if (eventType == 1 && getMode(MODE_Mouse1000)) {
        return;
    }

    if (cb == 3 && getMode(MODE_Mouse1002)) {
        return;
    }

    // With the exception of the 1006 mode, button release is encoded in cb.
    // Note that if multiple extensions are enabled, the 1006 is used, so it's okay to check for only that.
    if (eventType == 2 && !getMode(MODE_Mouse1006)) {
        cb = 3;
    }

    // normal buttons are passed as 0x20 + button,
    // mouse wheel (buttons 4,5) as 0x5c + button
    if (cb >= 4) {
        cb += 0x3c;
    }

    // Mouse motion handling
    if ((getMode(MODE_Mouse1002) || getMode(MODE_Mouse1003)) && eventType == 1) {
        cb += 0x20; // add 32 to signify motion event
    }
    char command[40];
    command[0] = '\0';
    // Check the extensions in decreasing order of preference. Encoding the release event above assumes that 1006 comes first.
    if (getMode(MODE_Mouse1006)) {
        snprintf(command, sizeof(command), "\033[<%d;%d;%d%c", cb, cx, cy, eventType == 2 ? 'm' : 'M');
    } else if (getMode(MODE_Mouse1015)) {
        snprintf(command, sizeof(command), "\033[%d;%d;%dM", cb + 0x20, cx, cy);
    } else if (getMode(MODE_Mouse1005)) {
        if (cx <= 2015 && cy <= 2015) {
            // The xterm extension uses UTF-8 (up to 2 bytes) to encode
            // coordinate+32, no matter what the locale is. We could easily
            // convert manually, but QString can also do it for us.
            QChar coords[2];
            coords[0] = QChar(cx + 0x20);
            coords[1] = QChar(cy + 0x20);
            QString coordsStr = QString(coords, 2);
            QByteArray utf8 = coordsStr.toUtf8();
            snprintf(command, sizeof(command), "\033[M%c%s", cb + 0x20, utf8.constData());
        }
    } else if (cx <= 223 && cy <= 223) {
        snprintf(command, sizeof(command), "\033[M%c%c%c", cb + 0x20, cx + 0x20, cy + 0x20);
    }

    sendString(command);
}

void Vt102Emulation::emulateUpDown(bool up, KeyboardTranslator::Entry entry, QByteArray &textToSend, int toCol)
{
    int cuX = _currentScreen->getCursorX();
    int cuY = _currentScreen->getCursorY();
    int realX = cuX;
    QVector<LineProperty> lineProperties = _currentScreen->getLineProperties(cuY - 1 + _currentScreen->getHistLines(),
                                                                             qMin(_currentScreen->getLines() - 1, cuY + 1) + _currentScreen->getHistLines());
    int num = _currentScreen->getColumns();
    if (up) {
        if ((lineProperties[0] & LINE_WRAPPED) == 0) {
            num = cuX + qMax(0, LineLength(lineProperties[0]) - cuX) + 1;
        }
    } else {
        if ((lineProperties[1] & LINE_WRAPPED) == 0 || (lineProperties[2] & LINE_WRAPPED) == 0) {
            realX = qMin(cuX, LineLength(lineProperties[2]) + 1);
            num = LineLength(lineProperties[1]) - cuX + realX;
        }
    }
    if (toCol > -1) {
        num += up ? realX - toCol : toCol - realX;
    }
    for (int i = 1; i < num; i++) {
        // One more will be added by the rest of the code.
        textToSend += entry.text();
    }
}

/**
 * The focus change event can be used by Vim (or other terminal applications)
 * to recognize that the konsole window has changed focus.
 * The escape sequence is also used by iTerm2.
 * Vim needs the following plugin to be installed to convert the escape
 * sequence into the FocusLost/FocusGained autocmd:
 * https://github.com/sjl/vitality.vim
 */
void Vt102Emulation::focusChanged(bool focused)
{
    if (_reportFocusEvents) {
        sendString(focused ? "\033[I" : "\033[O");
    }
}

void Vt102Emulation::sendText(const QString &text)
{
    if (!text.isEmpty()) {
        QKeyEvent event(QEvent::KeyPress, 0, Qt::NoModifier, text);
        sendKeyEvent(&event); // expose as a big fat keypress event
    }
}

void Vt102Emulation::sendKeyEvent(QKeyEvent *event)
{
    const Qt::KeyboardModifiers modifiers = event->modifiers();
    KeyboardTranslator::States states = KeyboardTranslator::NoState;

    TerminalDisplay *currentView = _currentScreen->currentTerminalDisplay();
    bool isReadOnly = false;
    if (currentView != nullptr && currentView->sessionController() != nullptr) {
        isReadOnly = currentView->sessionController()->isReadOnly();
    }

    // get current states
    if (getMode(MODE_NewLine)) {
        states |= KeyboardTranslator::NewLineState;
    }
    if (getMode(MODE_Ansi)) {
        states |= KeyboardTranslator::AnsiState;
    }
    if (getMode(MODE_AppCuKeys)) {
        states |= KeyboardTranslator::CursorKeysState;
    }
    if (getMode(MODE_AppScreen)) {
        states |= KeyboardTranslator::AlternateScreenState;
    }
    if (getMode(MODE_AppKeyPad) && ((modifiers & Qt::KeypadModifier) != 0U)) {
        states |= KeyboardTranslator::ApplicationKeypadState;
    }

    if (!isReadOnly) {
        // check flow control state
        if ((modifiers & Qt::ControlModifier) != 0U) {
            switch (event->key()) {
            case Qt::Key_S:
                Q_EMIT flowControlKeyPressed(true);
                break;
            case Qt::Key_C:
                if (m_SixelStarted) {
                    SixelModeAbort();
                }

                // Allow the user to take back control
                resetTokenizer();
                Q_EMIT flowControlKeyPressed(false);
                break;
            case Qt::Key_Q: // cancel flow control
                Q_EMIT flowControlKeyPressed(false);
                break;
            }
        }
    }
    // look up key binding
    if (_keyTranslator != nullptr) {
        KeyboardTranslator::Entry entry = _keyTranslator->findEntry(event->key(), modifiers, states);
        // send result to terminal
        QByteArray textToSend;

        int cuX = _currentScreen->getCursorX();
        int cuY = _currentScreen->getCursorY();
        if ((event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) && _currentScreen->replMode() == REPL_INPUT
            && _currentScreen->currentTerminalDisplay()->semanticUpDown()) {
            bool up = event->key() == Qt::Key_Up;
            if ((up && _currentScreen->replModeStart() <= std::make_pair(cuY - 1, cuX))
                || (!up && std::make_pair(cuY + 1, cuX) <= _currentScreen->replModeEnd())) {
                entry = _keyTranslator->findEntry(up ? Qt::Key_Left : Qt::Key_Right, Qt::NoModifier, states);
                emulateUpDown(up, entry, textToSend);
            }
        }

        // special handling for the Alt (aka. Meta) modifier.  pressing
        // Alt+[Character] results in Esc+[Character] being sent
        // (unless there is an entry defined for this particular combination
        //  in the keyboard modifier)
        const bool wantsAltModifier = ((entry.modifiers() & entry.modifierMask() & Qt::AltModifier) != 0U);
        const bool wantsMetaModifier = ((entry.modifiers() & entry.modifierMask() & Qt::MetaModifier) != 0U);
        const bool wantsAnyModifier = ((entry.state() & entry.stateMask() & KeyboardTranslator::AnyModifierState) != 0);

        if (((modifiers & Qt::AltModifier) != 0U) && !(wantsAltModifier || wantsAnyModifier) && !event->text().isEmpty()) {
            textToSend.prepend("\033");
        }
        if (((modifiers & Qt::MetaModifier) != 0U) && !(wantsMetaModifier || wantsAnyModifier) && !event->text().isEmpty()) {
            textToSend.prepend("\030@s");
        }

        if (entry.command() != KeyboardTranslator::NoCommand) {
            if ((entry.command() & KeyboardTranslator::EraseCommand) != 0) {
                textToSend += eraseChar();
            }
            if (currentView != nullptr) {
                if ((entry.command() & KeyboardTranslator::ScrollPageUpCommand) != 0) {
                    currentView->scrollScreenWindow(ScreenWindow::ScrollPages, -1);
                } else if ((entry.command() & KeyboardTranslator::ScrollPageDownCommand) != 0) {
                    currentView->scrollScreenWindow(ScreenWindow::ScrollPages, 1);
                } else if ((entry.command() & KeyboardTranslator::ScrollLineUpCommand) != 0) {
                    currentView->scrollScreenWindow(ScreenWindow::ScrollLines, -1);
                } else if ((entry.command() & KeyboardTranslator::ScrollLineDownCommand) != 0) {
                    currentView->scrollScreenWindow(ScreenWindow::ScrollLines, 1);
                } else if ((entry.command() & KeyboardTranslator::ScrollUpToTopCommand) != 0) {
                    currentView->scrollScreenWindow(ScreenWindow::ScrollLines, -currentView->screenWindow()->currentLine());
                } else if ((entry.command() & KeyboardTranslator::ScrollDownToBottomCommand) != 0) {
                    currentView->scrollScreenWindow(ScreenWindow::ScrollLines, lineCount());
                } else if ((entry.command() & KeyboardTranslator::ScrollPromptUpCommand) != 0) {
                    currentView->scrollScreenWindow(ScreenWindow::ScrollPrompts, -1);
                } else if ((entry.command() & KeyboardTranslator::ScrollPromptDownCommand) != 0) {
                    currentView->scrollScreenWindow(ScreenWindow::ScrollPrompts, 1);
                }
            }
        } else if (!entry.text().isEmpty()) {
            textToSend += entry.text(true, modifiers);
        } else {
            Q_ASSERT(_codec);
            textToSend += _codec->fromUnicode(event->text());
        }

        if (!isReadOnly) {
            Q_EMIT sendData(textToSend);
        }
    } else {
        if (!isReadOnly) {
            // print an error message to the terminal if no key translator has been
            // set
            QString translatorError = i18n(
                "No keyboard translator available.  "
                "The information needed to convert key presses "
                "into characters to send to the terminal "
                "is missing.");
            reset();
            receiveData(translatorError.toLatin1().constData(), translatorError.count());
        }
    }
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                VT100 Charsets                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

// Character Set Conversion ------------------------------------------------ --

/*
   The processing contains a VT100 specific code translation layer.
   It's still in use and mainly responsible for the line drawing graphics.

   These and some other glyphs are assigned to codes (0x5f-0xfe)
   normally occupied by the latin letters. Since this codes also
   appear within control sequences, the extra code conversion
   does not permute with the tokenizer and is placed behind it
   in the pipeline. It only applies to tokens, which represent
   plain characters.

   This conversion it eventually continued in TerminalDisplay.C, since
   it might involve VT100 enhanced fonts, which have these
   particular glyphs allocated in (0x00-0x1f) in their code page.
*/

#define CHARSET _charset[_currentScreen == _screen[1]]

// Apply current character map.

unsigned int Vt102Emulation::applyCharset(uint c)
{
    if (CHARSET.graphic && 0x5f <= c && c <= 0x7e) {
        return vt100_graphics[c - 0x5f];
    }
    if (CHARSET.pound && c == '#') {
        return 0xa3; // This mode is obsolete
    }
    return c;
}

/*
   "Charset" related part of the emulation state.
   This configures the VT100 charset filter.

   While most operation work on the current _screen,
   the following two are different.
*/

void Vt102Emulation::resetCharset(int scrno)
{
    _charset[scrno].cu_cs = 0;
    qstrncpy(_charset[scrno].charset, "BBBB", 4);
    _charset[scrno].sa_graphic = false;
    _charset[scrno].sa_pound = false;
    _charset[scrno].graphic = false;
    _charset[scrno].pound = false;
}

void Vt102Emulation::setCharset(int n, int cs) // on both screens.
{
    _charset[0].charset[n & 3] = cs;
    useCharset(_charset[0].cu_cs);
    _charset[1].charset[n & 3] = cs;
    useCharset(_charset[1].cu_cs);
}

void Vt102Emulation::setAndUseCharset(int n, int cs)
{
    CHARSET.charset[n & 3] = cs;
    useCharset(n & 3);
}

void Vt102Emulation::useCharset(int n)
{
    CHARSET.cu_cs = n & 3;
    CHARSET.graphic = (CHARSET.charset[n & 3] == '0');
    CHARSET.pound = (CHARSET.charset[n & 3] == 'A'); // This mode is obsolete
}

void Vt102Emulation::setDefaultMargins()
{
    _screen[0]->setDefaultMargins();
    _screen[1]->setDefaultMargins();
}

void Vt102Emulation::setMargins(int t, int b)
{
    _screen[0]->setMargins(t, b);
    _screen[1]->setMargins(t, b);
}

void Vt102Emulation::saveCursor()
{
    CHARSET.sa_graphic = CHARSET.graphic;
    CHARSET.sa_pound = CHARSET.pound; // This mode is obsolete
    // we are not clear about these
    // sa_charset = charsets[cScreen->_charset];
    // sa_charset_num = cScreen->_charset;
    _currentScreen->saveCursor();
}

void Vt102Emulation::restoreCursor()
{
    CHARSET.graphic = CHARSET.sa_graphic;
    CHARSET.pound = CHARSET.sa_pound; // This mode is obsolete
    _currentScreen->restoreCursor();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Mode Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*
   Some of the emulations state is either added to the state of the screens.

   This causes some scoping problems, since different emulations choose to
   located the mode either to the current _screen or to both.

   For strange reasons, the extend of the rendition attributes ranges over
   all screens and not over the actual _screen.

   We decided on the precise precise extend, somehow.
*/

// "Mode" related part of the state. These are all booleans.

void Vt102Emulation::resetModes()
{
    // MODE_Allow132Columns is not reset here
    // to match Xterm's behavior (see Xterm's VTReset() function)

    // MODE_Mouse1007 (Alternate Scrolling) is not reset here, to maintain
    // the profile alternate scrolling property after reset() is called, which
    // makes more sense; also this matches XTerm behavior.

    resetMode(MODE_132Columns);
    saveMode(MODE_132Columns);
    resetMode(MODE_Mouse1000);
    saveMode(MODE_Mouse1000);
    resetMode(MODE_Mouse1001);
    saveMode(MODE_Mouse1001);
    resetMode(MODE_Mouse1002);
    saveMode(MODE_Mouse1002);
    resetMode(MODE_Mouse1003);
    saveMode(MODE_Mouse1003);
    resetMode(MODE_Mouse1005);
    saveMode(MODE_Mouse1005);
    resetMode(MODE_Mouse1006);
    saveMode(MODE_Mouse1006);
    resetMode(MODE_Mouse1015);
    saveMode(MODE_Mouse1015);
    resetMode(MODE_BracketedPaste);
    saveMode(MODE_BracketedPaste);

    resetMode(MODE_AppScreen);
    saveMode(MODE_AppScreen);
    resetMode(MODE_AppCuKeys);
    saveMode(MODE_AppCuKeys);
    resetMode(MODE_AppKeyPad);
    saveMode(MODE_AppKeyPad);
    resetMode(MODE_NewLine);
    setMode(MODE_Ansi);
}

void Vt102Emulation::setMode(int m)
{
    _currentModes.mode[m] = true;
    switch (m) {
    case MODE_132Columns:
        if (getMode(MODE_Allow132Columns)) {
            clearScreenAndSetColumns(132);
        } else {
            _currentModes.mode[m] = false;
        }
        break;
    case MODE_Mouse1000:
    case MODE_Mouse1001:
    case MODE_Mouse1002:
    case MODE_Mouse1003:
        _currentModes.mode[MODE_Mouse1000] = false;
        _currentModes.mode[MODE_Mouse1001] = false;
        _currentModes.mode[MODE_Mouse1002] = false;
        _currentModes.mode[MODE_Mouse1003] = false;
        _currentModes.mode[m] = true;
        Q_EMIT programRequestsMouseTracking(true);
        break;
    case MODE_Mouse1007:
        Q_EMIT enableAlternateScrolling(true);
        break;
    case MODE_Mouse1005:
    case MODE_Mouse1006:
    case MODE_Mouse1015:
        _currentModes.mode[MODE_Mouse1005] = false;
        _currentModes.mode[MODE_Mouse1006] = false;
        _currentModes.mode[MODE_Mouse1015] = false;
        _currentModes.mode[m] = true;
        break;

    case MODE_BracketedPaste:
        Q_EMIT programBracketedPasteModeChanged(true);
        break;

    case MODE_AppScreen:
        _screen[1]->setDefaultRendition();
        _screen[1]->clearSelection();
        setScreen(1);
        if (_currentScreen && _currentScreen->currentTerminalDisplay()) {
            _currentScreen->delPlacements(1);
            _currentScreen->currentTerminalDisplay()->update();
        }
        break;
    }
    // FIXME: Currently this has a redundant condition as MODES_SCREEN is 6
    // and MODE_NewLine is 5
    if (m < MODES_SCREEN || m == MODE_NewLine) {
        _screen[0]->setMode(m);
        _screen[1]->setMode(m);
    }
}

void Vt102Emulation::resetMode(int m)
{
    _currentModes.mode[m] = false;
    switch (m) {
    case MODE_132Columns:
        if (getMode(MODE_Allow132Columns)) {
            clearScreenAndSetColumns(80);
        }
        break;
    case MODE_Mouse1000:
    case MODE_Mouse1001:
    case MODE_Mouse1002:
    case MODE_Mouse1003:
        // Same behavior as xterm, these modes are mutually exclusive,
        // and disabling any disables mouse tracking.
        _currentModes.mode[MODE_Mouse1000] = false;
        _currentModes.mode[MODE_Mouse1001] = false;
        _currentModes.mode[MODE_Mouse1002] = false;
        _currentModes.mode[MODE_Mouse1003] = false;
        Q_EMIT programRequestsMouseTracking(false);
        break;
    case MODE_Mouse1007:
        Q_EMIT enableAlternateScrolling(false);
        break;

    case MODE_BracketedPaste:
        Q_EMIT programBracketedPasteModeChanged(false);
        break;

    case MODE_AppScreen:
        _screen[0]->clearSelection();
        setScreen(0);
        if (_currentScreen && _currentScreen->currentTerminalDisplay()) {
            _currentScreen->currentTerminalDisplay()->update();
        }
        break;
    }
    // FIXME: Currently this has a redundant condition as MODES_SCREEN is 7
    // MODE_AppScreen is 6 and MODE_NewLine is 5
    if (m < MODES_SCREEN || m == MODE_NewLine) {
        _screen[0]->resetMode(m);
        _screen[1]->resetMode(m);
    }
}

void Vt102Emulation::saveMode(int m)
{
    _savedModes.mode[m] = _currentModes.mode[m];
}

void Vt102Emulation::restoreMode(int m)
{
    if (_savedModes.mode[m]) {
        setMode(m);
    } else {
        resetMode(m);
    }
}

bool Vt102Emulation::getMode(int m)
{
    return _currentModes.mode[m];
}

char Vt102Emulation::eraseChar() const
{
    KeyboardTranslator::Entry entry = _keyTranslator->findEntry(Qt::Key_Backspace, Qt::NoModifier, KeyboardTranslator::NoState);
    if (entry.text().count() > 0) {
        return entry.text().at(0);
    } else {
        return '\b';
    }
}

// return contents of the scan buffer
static QString hexdump2(uint *s, int len)
{
    int i;
    char dump[128];
    QString returnDump;

    for (i = 0; i < len; i++) {
        if (s[i] == '\\') {
            snprintf(dump, sizeof(dump), "%s", "\\\\");
        } else if ((s[i]) > 32 && s[i] < 127) {
            snprintf(dump, sizeof(dump), "%c", s[i]);
        } else if (s[i] == 0x1b) {
            snprintf(dump, sizeof(dump), "ESC");
        } else {
            snprintf(dump, sizeof(dump), "\\%04x(hex)", s[i]);
        }
        returnDump.append(QLatin1String(dump));
    }
    return returnDump;
}

void Vt102Emulation::reportDecodingError(int token)
{
    QString outputError = QStringLiteral("Undecodable sequence: ");

    switch (token & 0xff) {
    case 2:
    case 3:
    case 4:
        outputError.append(QStringLiteral("ESC "));
        break;
    case 5:
    case 6:
    case 7:
    case 9:
    case 10:
    case 11:
    case 12:
    case 13:
        outputError.append(QStringLiteral("CSI "));
        break;
    case 14:
        outputError.append(QStringLiteral("OSC "));
        break;
    case 15:
        outputError.append(QStringLiteral("APC "));
        break;
    }
    if ((token & 0xff) != 8) { // VT52
        outputError.append(hexdump2(tokenBuffer, tokenBufferPos));
    } else {
        outputError.append(QStringLiteral("(VT52) ESC"));
    }

    if ((token & 0xff) != 3) { // SCS
        outputError.append((token >> 8) & 0xff);
    } else {
        outputError.append((token >> 16) & 0xff);
    }

    qCDebug(KonsoleDebug).noquote() << outputError;

    resetTokenizer();

    if (m_SixelStarted) {
        SixelModeAbort();
    }
}

void Vt102Emulation::sixelQuery(int q)
{
    char tmp[30];
    if (q == 1) {
        if (params.value[1] == 1 || params.value[1] == 4) {
            snprintf(tmp, sizeof(tmp), "\033[?1;0;%dS", MAX_SIXEL_COLORS);
            sendString(tmp);
        }
    }
    if (q == 2) {
        if (params.value[1] == 1 || params.value[1] == 4) {
            snprintf(tmp, sizeof(tmp), "\033[?2;0;%d;%dS", MAX_IMAGE_DIM, MAX_IMAGE_DIM);
            sendString(tmp);
        }
    }
}

void Vt102Emulation::SixelModeEnable(int width, int height)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    if (width > MAX_IMAGE_DIM) {
        width = MAX_IMAGE_DIM;
    }
    if (height > MAX_IMAGE_DIM) {
        height = MAX_IMAGE_DIM;
    }
    m_actualSize = QSize(width, height);

    // We assume square pixels because I'm lazy and didn't implement the stuff in vt102emulation.
    int charactersHeight = height / 6 + 1;

    m_currentImage = QImage(width, charactersHeight * 6 + 1, QImage::Format_Indexed8);
    m_currentColor = 3;
    m_currentX = 0;
    m_verticalPosition = 0;
    if (!m_currentImage.isNull()) {
        m_SixelStarted = true;
    }
    m_currentImage.fill(0);
    std::string initial_colors[16] = {"#000000",
                                      "#3333CC",
                                      "#CC2323",
                                      "#33CC33",
                                      "#CC33CC",
                                      "#33CCCC",
                                      "#CCCC33",
                                      "#777777",
                                      "#444444",
                                      "#565699",
                                      "#994444",
                                      "#569956",
                                      "#995699",
                                      "#569999",
                                      "#999956",
                                      "#CCCCCC"};
    for (int i = 0; i < 16; i++) {
        m_currentImage.setColor(i, QColor(initial_colors[i].c_str()).rgb());
    }
}

void Vt102Emulation::SixelModeAbort()
{
    if (!m_SixelStarted) {
        return;
    }
    resetMode(MODE_Sixel);
    resetTokenizer();
    m_SixelStarted = false;
    m_currentImage = QImage();
}

void Vt102Emulation::SixelModeDisable()
{
    if (!m_SixelStarted) {
        return;
    }
    m_SixelStarted = false;
    int col, row;
    if (m_SixelScrolling) {
        col = _currentScreen->getCursorX();
        row = _currentScreen->getCursorY();
    } else {
        col = 0;
        row = 0;
    }
    QPixmap pixmap = QPixmap::fromImage(m_currentImage.copy(QRect(0, 0, m_actualSize.width(), m_actualSize.height())));
    if (m_aspect.first != m_aspect.second) {
        pixmap = pixmap.scaled(pixmap.width(), m_aspect.first * pixmap.height() / m_aspect.second);
    }
    int rows = -1, cols = -1;
    _currentScreen->addPlacement(pixmap, rows, cols, row, col, m_SixelScrolling, m_SixelScrolling * 2, false);
}

void Vt102Emulation::SixelColorChangeRGB(const int index, int red, int green, int blue)
{
    if (index < 0 || index >= MAX_SIXEL_COLORS) {
        return;
    }

    red = red * 255 / 100;
    green = green * 255 / 100;
    blue = blue * 255 / 100;

    // QImage automatically handles the size of the color table
    m_currentImage.setColor(index, qRgb(red, green, blue));
    m_currentColor = index;
}

void Vt102Emulation::SixelColorChangeHSL(const int index, int hue, int saturation, int value)
{
    if (index < 0 || index >= MAX_SIXEL_COLORS) {
        return;
    }

    hue = qBound(0, hue, 360);
    saturation = qBound(0, saturation, 100);
    value = qBound(0, value, 100);

    // libsixel is offset by 240 degrees, so we assume that is correct
    hue = (hue + 240) % 360;

    saturation = saturation * 255 / 100;
    value = value * 255 / 100;

    m_currentImage.setColor(index, QColor::fromHsl(hue, saturation, value).rgb());
    m_currentColor = index;
}

void Vt102Emulation::SixelCharacterAdd(uint8_t character, int repeat)
{
    if (!m_SixelStarted) {
        return;
    }

    switch (character) {
    case '\r':
        m_currentX = 0;
        return;
    case '\n':
        m_verticalPosition++;
        return;
    default:
        break;
    }
    character -= '?';
    const int top = m_verticalPosition * 6;
    const int bottom = (m_verticalPosition + 1) * 6;
    if (bottom > MAX_IMAGE_DIM) { // Ignore lines below MAX_IMAGE_DIM
        return;
    }
    repeat = std::clamp(repeat, 1, MAX_IMAGE_DIM - m_currentX); // Won't repeat beyond MAX_IMAGE_DIM

    if (bottom >= m_currentImage.height() - 1 || m_currentX + repeat >= m_currentImage.width()) {
        // If we copy out of bounds it gets filled with 0
        int extraWidth = 255 + repeat; // Increase size by at least 256, to avoid increasing for every pixel
        int newWidth = qMax(m_currentX + extraWidth, m_currentImage.width() + extraWidth);
        int newHeight = (qMax(bottom + 256, m_currentImage.height() + 256) / 6 + 1) * 6;
        newWidth = qMin(newWidth, MAX_IMAGE_DIM);
        newHeight = qMin(newHeight, MAX_IMAGE_DIM);
        if (newWidth != m_currentImage.width() || newHeight != m_currentImage.height()) {
            m_currentImage = m_currentImage.copy(0, 0, newWidth, newHeight);
        }
        if (m_currentImage.isNull()) {
            m_SixelStarted = false;
            return;
        }
    }

    const ptrdiff_t bpl = m_currentImage.bytesPerLine();
    uchar *data = m_currentImage.bits() + top * bpl + m_currentX;

    if (repeat == 1) {
        if (m_preserveBackground) {
            // Konsole is built without _any_ optimizations by default, so a little
            // manual unrolling to avoid calling shift for every loop iteration.
            if (character & (1 << 0)) {
                data[0 * bpl] = m_currentColor;
            }
            if (character & (1 << 1)) {
                data[1 * bpl] = m_currentColor;
            }
            if (character & (1 << 2)) {
                data[2 * bpl] = m_currentColor;
            }
            if (character & (1 << 3)) {
                data[3 * bpl] = m_currentColor;
            }
            if (character & (1 << 4)) {
                data[4 * bpl] = m_currentColor;
            }
            if (character & (1 << 5)) {
                data[5 * bpl] = m_currentColor;
            }
        } else {
            for (int i = 0; i < 6; i++, data += bpl) {
                *data = (character & (1 << i)) * m_currentColor;
            }
        }
        m_currentX++;
    } else {
        if (m_preserveBackground) {
            // Konsole is built without _any_ optimizations by default, so a little
            // manual unrolling to avoid calling shift for every loop iteration.
            if (character & (1 << 0)) {
                memset(&data[0 * bpl], m_currentColor, repeat);
            }
            if (character & (1 << 1)) {
                memset(&data[1 * bpl], m_currentColor, repeat);
            }
            if (character & (1 << 2)) {
                memset(&data[2 * bpl], m_currentColor, repeat);
            }
            if (character & (1 << 3)) {
                memset(&data[3 * bpl], m_currentColor, repeat);
            }
            if (character & (1 << 4)) {
                memset(&data[4 * bpl], m_currentColor, repeat);
            }
            if (character & (1 << 5)) {
                memset(&data[5 * bpl], m_currentColor, repeat);
            }
        } else {
            for (int i = 0; i < 6; i++, data += bpl) {
                memset(data, (character & (1 << i)) * m_currentColor, repeat);
            }
        }
        m_currentX += repeat;
    }
    if (m_currentX > m_actualSize.width()) {
        m_actualSize.setWidth(m_currentX);
    }
    if (bottom > m_actualSize.height()) {
        m_actualSize.setHeight(bottom);
    }
}

bool Vt102Emulation::processSixel(uint cc)
{
    switch (cc) {
    case '$':
        SixelCharacterAdd('\r');
        resetTokenizer();
        return true;
    case '-':
        SixelCharacterAdd('\r');
        SixelCharacterAdd('\n');
        resetTokenizer();
        return true;
    default:
        break;
    }
    uint *s = tokenBuffer;
    const int p = tokenBufferPos;

    if (!m_SixelStarted && (sixel() || s[0] == '!' || s[0] == '#')) {
        m_aspect = qMakePair(1, 1);
        SixelModeEnable(30, 6);
    }
    if (sixel()) {
        SixelCharacterAdd(uint8_t(cc));
        resetTokenizer();
        return true;
    }
    if (ccc(DIG)) {
        addDigit(cc - '0');
        return true;
    }
    if (cc == ';') {
        addArgument();
        return true;
    }

    if (s[0] == '"') {
        if (p < 3) {
            return true;
        }
        addArgument();

        if (params.count == 4 || params.count == 2) {
            // We just ignore the pixel aspect ratio, it's dumb
            // const int pixelWidth = params.value[0];
            // const int pixelHeight = params.value[1];

            if (!m_SixelStarted) {
                if (params.value[1] == 0 || params.value[0] == 0) {
                    m_aspect = qMakePair(1, 1);
                } else {
                    m_aspect = qMakePair(params.value[0], params.value[1]);
                }
                int width;
                int height;
                if (params.count == 4) {
                    width = params.value[2];
                    height = params.value[3];
                } else {
                    // Default size
                    width = 8;
                    height = 6;
                }
                SixelModeEnable(width, height);
            }
            resetTokenizer();
            receiveChars(QVector<uint>{cc}); // re-send the actual character
            return true;
        }
        return false;
    }

    // Repeat character
    if (s[0] == '!') {
        if (p < 2) {
            return true;
        }
        if (ces(DIG)) {
            addDigit(cc - '0');
            return true;
        }

        SixelCharacterAdd(cc, params.value[0]);
        resetTokenizer();
        return true;
    }

    if (s[0] == '#') {
        if (p < 2) {
            return true;
        }
        addArgument();
        if (params.count < 1) {
            return false;
        }
        const int index = params.value[0];
        if (params.count == 5) {
            const int colorspace = params.value[1];
            switch (colorspace) {
            case 1:
                // Confusingly it is in HLS order...
                SixelColorChangeHSL(index, params.value[2], params.value[4], params.value[3]);
                break;
            case 2:
                SixelColorChangeRGB(index, params.value[2], params.value[3], params.value[4]);
                break;
            default:
                return false;
            }
        } else if (params.count == 1 && index >= 0) { // Negative index is an error. Too large index is ignored
            if (index < MAX_SIXEL_COLORS) {
                m_currentColor = index;
            }
        } else {
            return false;
        }
        resetTokenizer();
        receiveChars(QVector<uint>{cc}); // re-send the actual character
        return true;
    }
    return false;
}

int Vt102Emulation::getFreeGraphicsImageId()
{
    int i = 1;
    while (1) {
        if (!_graphicsImages.count(i)) {
            return i;
        }
        i++;
    }
}
