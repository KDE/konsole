/* ------------------------------------------------------------------------- */
/*                                                                           */
/* [vt102emu.cpp]              VT102 Terminal Emulation                      */
/*                                                                           */
/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>           */
/*                                                                           */
/* This file is part of Konsole - an X terminal for KDE                      */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*! \class VT102Emulation

   \brief Actual Emulation for Konsole

   This class is responsible to scan the escapes sequences of the terminal
   emulation and to map it to their corresponding semantic complements.
   Thus this module knows mainly about decoding escapes sequences and
   is a stateless device w.r.t. the semantics.

   It is also responsible to refresh the TEWidget by certain rules.

   \sa TEWidget \sa TEScreen
*/

/* FIXME
   - For strange reasons, the extend of the rendition attributes ranges over
     all screens and not over the actual screen. We have to find out the
     precise extend.
*/
//NOTE: search for 'NotImplemented' and IGNORED to find unimplemented features.
//TODO: we have no means yet, to change the emulation on the fly.

#include "TEmuVt102.h"
#include "TEWidget.h"
#include "TEScreen.h"

#include <stdio.h>
#include <unistd.h>
#include <qkeycode.h>

#include <assert.h>

#include "TEmuVt102.moc"

#define ESC 27
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Emulation                                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#define CNTL(c) ((c)-'@')

/*!
*/

int maxHistLines = 2000;
//#define MAXHISTLINES 100 // natural constant for now.

VT102Emulation::VT102Emulation(TEWidget* gui, const char* term) : Emulation(gui)
{
  screen[0] = scr;
  screen[0]->setHistMaxLines(maxHistLines);
  screen[1] = new TEScreen(gui->Lines(),gui->Columns());
  // note that the application screen has not history buffer.

  QObject::connect(gui,SIGNAL(mouseSignal(int,int,int)),
                   this,SLOT(onMouse(int,int,int)));

  tableInit();
  resetTerminal();
  setMode(MODE_BsHack);
  emulation = term;
}

void VT102Emulation::resetTerminal()
{

  reset();

  resetMode(MODE_Mouse1000); saveMode(MODE_Mouse1000);
  resetMode(MODE_AppCuKeys); saveMode(MODE_AppCuKeys);
  resetMode(MODE_AppScreen); saveMode(MODE_AppScreen);
  resetMode(MODE_NewLine);
    setMode(MODE_Ansi);

  screen[0]->reset();
  screen[1]->reset();
}

/*!
*/

VT102Emulation::~VT102Emulation()
{
  scr = screen[0];
  delete screen[1];
}


void VT102Emulation::onImageSizeChange(int lines, int columns)
{
  if (scr != screen[0]) screen[0]->resizeImage(lines,columns);
  if (scr != screen[1]) screen[1]->resizeImage(lines,columns);
  Emulation::onImageSizeChange(lines,columns);
}

void VT102Emulation::NewLine()
{
  scr->NewLine(); bulkNewline();
}

void VT102Emulation::setColumns(int columns)
{
  emit changeColumns(columns); // this goes strange ways
}

// Interpreting Codes ---------------------------------------------------------
/*
   This section deals with decoding the incoming character stream.
   Decoding means here, that the stream is first seperated into `tokens'
   which are then mapped to a `meaning' provided as operations by the
   `Screen' class.

   The tokens are defined below. They are:

   - CHR        - Printable characters     (32..255 but DEL (=127))
   - CTL        - Control characters       (0..31 but ESC (= 27), DEL)
   - ESC        - Escape codes of the form <ESC><CHR but `[]()+*#'>
   - ESC_DE     - Escape codes of the form <ESC><any of `()+*#'> C
   - CSI_PN     - Escape codes of the form <ESC>'['     {Pn} ';' {Pn} C
   - CSI_PS     - Escape codes of the form <ESC>'['     {Pn} ';' ...  C
   - CSI_PR     - Escape codes of the form <ESC>'[' '?' {Pn} ';' ...  C
   - VT52       - VT52 escape codes
                  - <ESC><Chr>
                  - <ESC>'Y'{Pc}{Pc}
   - XTE_HA     - Xterm hacks              <ESC>`]' {Pn} `;' {Text} <BEL>
                  note that this is handled differently

   The last two forms allow list of arguments. Since the elements of
   the lists are treated individually the same way, they are passed
   as individual tokens to the interpretation. Further, because the
   meaning of the parameters are names (althought represented as numbers),
   they are includes within the token ('N').

*/

#define TY_CONSTR(T,A,N) ( ((((int)N) & 0xffff) << 16) | ((((int)A) & 0xff) << 8) | (((int)T) & 0xff) )

#define TY_CHR___(   )  TY_CONSTR(0,0,0)
#define TY_CTL___(A  )  TY_CONSTR(1,A,0)
#define TY_ESC___(A  )  TY_CONSTR(2,A,0)
#define TY_ESC_CS(   )  TY_CONSTR(3,0,0)
#define TY_ESC_DE(A  )  TY_CONSTR(4,A,0)
#define TY_CSI_PS(A,N)  TY_CONSTR(5,A,N)
#define TY_CSI_PN(A  )  TY_CONSTR(6,A,0)
#define TY_CSI_PR(A,N)  TY_CONSTR(7,A,N)

#define TY_VT52__(A  )  TY_CONSTR(8,A,0)

//FIXME: include recognition of remaining VT100 codes.
//       this are printing modes, ESC[{ps}i (printing) and VT52 printing.

void VT102Emulation::tau( int code, int p, int q )
{
//scan_buffer_report();
//if (code == TY_CHR___()) printf("%c",p); else
//printf("tau(%d,%d,%d, %d,%d)\n",(code>>0)&0xff,(code>>8)&0xff,(code>>16)&0xffff,p,q);
  switch (code)
  {
    case TY_CHR___(         ) : scr->ShowCharacter        (p         ); break; //VT100

    //FIXME:       127 DEL    : ignored on input?

    case TY_CTL___(CNTL('@')) : /* NUL: ignored                      */ break;
    case TY_CTL___(CNTL('A')) : /* SOH: ignored                      */ break;
    case TY_CTL___(CNTL('B')) : /* STX: ignored                      */ break;
    case TY_CTL___(CNTL('C')) : /* ETX: ignored                      */ break;
    case TY_CTL___(CNTL('D')) : /* EOT: ignored                      */ break;
    case TY_CTL___(CNTL('E')) :      reportAnswerBack     (          ); break; //VT100
    case TY_CTL___(CNTL('F')) : /* ACK: ignored                      */ break;
    case TY_CTL___(CNTL('G')) : gui->Bell                 (          ); break; //VT100
    case TY_CTL___(CNTL('H')) : scr->BackSpace            (          ); break; //VT100
    case TY_CTL___(CNTL('I')) : scr->Tabulate             (          ); break; //VT100
    case TY_CTL___(CNTL('J')) :      NewLine              (          ); break; //VT100
    case TY_CTL___(CNTL('K')) :      NewLine              (          ); break; //VT100
    case TY_CTL___(CNTL('L')) :      NewLine              (          ); break; //VT100
    case TY_CTL___(CNTL('M')) : scr->Return               (          ); break; //VT100
    case TY_CTL___(CNTL('N')) : scr->useCharset           (         1); break; //VT100
    case TY_CTL___(CNTL('O')) : scr->useCharset           (         0); break; //VT100
    case TY_CTL___(CNTL('P')) : /* DLE: ignored                      */ break;
    case TY_CTL___(CNTL('Q')) : /* DC1: FIXME: XON continue          */ break; //VT100
    case TY_CTL___(CNTL('R')) : /* DC2: ignored                      */ break;
    case TY_CTL___(CNTL('S')) : /* DC3: XOFF halt                    */ break; //VT100
    case TY_CTL___(CNTL('T')) : /* DC4: ignored                      */ break;
    case TY_CTL___(CNTL('U')) : /* NAK: ignored                      */ break;
    case TY_CTL___(CNTL('V')) : /* SYN: ignored                      */ break;
    case TY_CTL___(CNTL('W')) : /* ETB: ignored                      */ break;
    case TY_CTL___(CNTL('X')) : scr->ShowCharacter        (         2); break; //VT100
    case TY_CTL___(CNTL('Y')) : /* EM : ignored                      */ break;
    case TY_CTL___(CNTL('Z')) : scr->ShowCharacter        (         2); break; //VT100
    case TY_CTL___(CNTL('[')) : /* ESC: cannot be seen here.         */ break;
    case TY_CTL___(CNTL('\\')): /* FS : ignored                      */ break;
    case TY_CTL___(CNTL(']')) : /* GS : ignored                      */ break;
    case TY_CTL___(CNTL('^')) : /* RS : ignored                      */ break;
    case TY_CTL___(CNTL('_')) : /* US : ignored                      */ break;

    case TY_ESC___('D'      ) : scr->index                (          ); break; //VT100
    case TY_ESC___('E'      ) : scr->NextLine             (          ); break; //VT100
    case TY_ESC___('H'      ) : scr->changeTabStop        (TRUE      ); break; //VT100
    case TY_ESC___('M'      ) : scr->reverseIndex         (          ); break; //VT100
    case TY_ESC___('Z'      ) :      reportTerminalType   (          ); break;
    case TY_ESC___('c'      ) :      resetTerminal        (          ); break;
    case TY_ESC___('n'      ) : scr->useCharset           (         2); break;
    case TY_ESC___('o'      ) : scr->useCharset           (         3); break;
    case TY_ESC___('7'      ) : scr->saveCursor           (          ); break;
    case TY_ESC___('8'      ) : scr->restoreCursor        (          ); break;
    case TY_ESC___('='      ) :          setMode      (MODE_AppKeyPad); break;
    case TY_ESC___('>'      ) :        resetMode      (MODE_AppKeyPad); break;
    case TY_ESC___('<'      ) :          setMode      (MODE_Ansi     ); break; //VT100

    case TY_ESC_CS(         ) :      setCharset           (p-'(',   q); break; //VT100

    case TY_ESC_DE('3'      ) : /* IGNORED: double high, top half    */ break;
    case TY_ESC_DE('4'      ) : /* IGNORED: double high, bottom half */ break;
    case TY_ESC_DE('5'      ) : /* IGNORED: single width, single high*/ break;
    case TY_ESC_DE('6'      ) : /* IGNORED: double width, single high*/ break;
    case TY_ESC_DE('8'      ) : scr->helpAlign            (          ); break;

    case TY_CSI_PS('K',    0) : scr->clearToEndOfLine     (          ); break;
    case TY_CSI_PS('K',    1) : scr->clearToBeginOfLine   (          ); break;
    case TY_CSI_PS('K',    2) : scr->clearEntireLine      (          ); break;
    case TY_CSI_PS('J',    0) : scr->clearToEndOfScreen   (          ); break;
    case TY_CSI_PS('J',    1) : scr->clearToBeginOfScreen (          ); break;
    case TY_CSI_PS('J',    2) : scr->clearEntireScreen    (          ); break;
    case TY_CSI_PS('g',    0) : scr->changeTabStop        (FALSE     ); break; //VT100
    case TY_CSI_PS('g',    3) : scr->clearTabStops        (          ); break; //VT100
    case TY_CSI_PS('h',    4) : scr->    setMode      (MODE_Insert   ); break;
    case TY_CSI_PS('h',   20) :          setMode      (MODE_NewLine  ); break;
    case TY_CSI_PS('i',    0) : /*                                   */ break; //VT100
    case TY_CSI_PS('l',    4) : scr->  resetMode      (MODE_Insert   ); break;
    case TY_CSI_PS('l',   20) :        resetMode      (MODE_NewLine  ); break;

    case TY_CSI_PS('m',    0) : scr->setDefaultRendition  (          ); break;
    case TY_CSI_PS('m',    1) : scr->  setRendition     (RE_BOLD     ); break; //VT100
    case TY_CSI_PS('m',    4) : scr->  setRendition     (RE_UNDERLINE); break; //VT100
    case TY_CSI_PS('m',    5) : scr->  setRendition     (RE_BLINK    ); break; //VT100
    case TY_CSI_PS('m',    7) : scr->  setRendition     (RE_REVERSE  ); break;
    case TY_CSI_PS('m',   10) : /* IGNORED: mapping related          */ break; //LINUX
    case TY_CSI_PS('m',   11) : /* IGNORED: mapping related          */ break; //LINUX
    case TY_CSI_PS('m',   12) : /* IGNORED: mapping related          */ break; //LINUX
    case TY_CSI_PS('m',   22) : scr->resetRendition     (RE_BOLD     ); break;
    case TY_CSI_PS('m',   24) : scr->resetRendition     (RE_UNDERLINE); break;
    case TY_CSI_PS('m',   25) : scr->resetRendition     (RE_BLINK    ); break;
    case TY_CSI_PS('m',   27) : scr->resetRendition     (RE_REVERSE  ); break;

    case TY_CSI_PS('m',   30) : scr->setForeColor         (         0); break;
    case TY_CSI_PS('m',   31) : scr->setForeColor         (         1); break;
    case TY_CSI_PS('m',   32) : scr->setForeColor         (         2); break;
    case TY_CSI_PS('m',   33) : scr->setForeColor         (         3); break;
    case TY_CSI_PS('m',   34) : scr->setForeColor         (         4); break;
    case TY_CSI_PS('m',   35) : scr->setForeColor         (         5); break;
    case TY_CSI_PS('m',   36) : scr->setForeColor         (         6); break;
    case TY_CSI_PS('m',   37) : scr->setForeColor         (         7); break;
    case TY_CSI_PS('m',   39) : scr->setForeColorToDefault(          ); break;

    case TY_CSI_PS('m',   40) : scr->setBackColor         (         0); break;
    case TY_CSI_PS('m',   41) : scr->setBackColor         (         1); break;
    case TY_CSI_PS('m',   42) : scr->setBackColor         (         2); break;
    case TY_CSI_PS('m',   43) : scr->setBackColor         (         3); break;
    case TY_CSI_PS('m',   44) : scr->setBackColor         (         4); break;
    case TY_CSI_PS('m',   45) : scr->setBackColor         (         5); break;
    case TY_CSI_PS('m',   46) : scr->setBackColor         (         6); break;
    case TY_CSI_PS('m',   47) : scr->setBackColor         (         7); break;
    case TY_CSI_PS('m',   49) : scr->setBackColorToDefault(          ); break;

    case TY_CSI_PS('m',   90) : scr->setForeColor         (         8); break;
    case TY_CSI_PS('m',   91) : scr->setForeColor         (         9); break;
    case TY_CSI_PS('m',   92) : scr->setForeColor         (        10); break;
    case TY_CSI_PS('m',   93) : scr->setForeColor         (        11); break;
    case TY_CSI_PS('m',   94) : scr->setForeColor         (        12); break;
    case TY_CSI_PS('m',   95) : scr->setForeColor         (        13); break;
    case TY_CSI_PS('m',   96) : scr->setForeColor         (        14); break;
    case TY_CSI_PS('m',   97) : scr->setForeColor         (        15); break;

    case TY_CSI_PS('m',  100) : scr->setBackColor         (         8); break;
    case TY_CSI_PS('m',  101) : scr->setBackColor         (         9); break;
    case TY_CSI_PS('m',  102) : scr->setBackColor         (        10); break;
    case TY_CSI_PS('m',  103) : scr->setBackColor         (        11); break;
    case TY_CSI_PS('m',  104) : scr->setBackColor         (        12); break;
    case TY_CSI_PS('m',  105) : scr->setBackColor         (        13); break;
    case TY_CSI_PS('m',  106) : scr->setBackColor         (        14); break;
    case TY_CSI_PS('m',  107) : scr->setBackColor         (        15); break;

    case TY_CSI_PS('n',    5) :      reportStatus         (          ); break;
    case TY_CSI_PS('n',    6) :      reportCursorPosition (          ); break;
    case TY_CSI_PS('q',    0) : /* IGNORED: LEDs off                 */ break; //VT100
    case TY_CSI_PS('q',    1) : /* IGNORED: LED1 on                  */ break; //VT100
    case TY_CSI_PS('q',    2) : /* IGNORED: LED2 on                  */ break; //VT100
    case TY_CSI_PS('q',    3) : /* IGNORED: LED3 on                  */ break; //VT100
    case TY_CSI_PS('q',    4) : /* IGNORED: LED4 on                  */ break; //VT100
    case TY_CSI_PS('x',    0) :      reportTerminalParms  (         2); break; //VT100
    case TY_CSI_PS('x',    1) :      reportTerminalParms  (         3); break; //VT100

    case TY_CSI_PN('@'      ) : scr->insertChars          (p         ); break;
    case TY_CSI_PN('A'      ) : scr->cursorUp             (p         ); break; //VT100
    case TY_CSI_PN('B'      ) : scr->cursorDown           (p         ); break; //VT100
    case TY_CSI_PN('C'      ) : scr->cursorRight          (p         ); break; //VT100
    case TY_CSI_PN('D'      ) : scr->cursorLeft           (p         ); break; //VT100
    case TY_CSI_PN('G'      ) : scr->setCursorX           (p         ); break; //LINUX
    case TY_CSI_PN('H'      ) : scr->setCursorYX          (p,       q); break; //VT100
    case TY_CSI_PN('L'      ) : scr->insertLines          (p         ); break;
    case TY_CSI_PN('M'      ) : scr->deleteLines          (p         ); break;
    case TY_CSI_PN('P'      ) : scr->deleteChars          (p         ); break;
    case TY_CSI_PN('X'      ) : scr->eraseChars           (p         ); break;
    case TY_CSI_PN('c'      ) :      reportTerminalType   (          ); break; //VT100
    case TY_CSI_PN('d'      ) : scr->setCursorY           (p         ); break; //LINUX
    case TY_CSI_PN('f'      ) : scr->setCursorYX          (p,       q); break; //VT100
    case TY_CSI_PN('r'      ) : scr->setMargins           (p,       q); break; //VT100
    case TY_CSI_PN('y'      ) : /* IGNORED: Confidence test          */ break; //VT100

    //FIXME: experimental, evtl. we can shrink this a little. (hlsr as parm?)
    case TY_CSI_PR('h',    1) :          setMode      (MODE_AppCuKeys); break; //VT100
    case TY_CSI_PR('l',    1) :        resetMode      (MODE_AppCuKeys); break; //VT100
    case TY_CSI_PR('s',    1) :         saveMode      (MODE_AppCuKeys); break; //FIXME
    case TY_CSI_PR('r',    1) :      restoreMode      (MODE_AppCuKeys); break; //FIXME
    case TY_CSI_PR('l',    2) :        resetMode      (MODE_Ansi     ); break; //VT100
    case TY_CSI_PR('h',    3) :      setColumns           (       132); break; //VT100
    case TY_CSI_PR('l',    3) :      setColumns           (        80); break; //VT100
    case TY_CSI_PR('h',    4) : /* IGNORED: soft scrolling           */ break; //VT100
    case TY_CSI_PR('l',    4) : /* IGNORED: soft scrolling           */ break; //VT100
    case TY_CSI_PR('h',    5) : scr->    setMode      (MODE_Screen   ); break; //VT100
    case TY_CSI_PR('l',    5) : scr->  resetMode      (MODE_Screen   ); break; //VT100
    case TY_CSI_PR('h',    6) : scr->    setMode      (MODE_Origin   ); break; //VT100
    case TY_CSI_PR('l',    6) : scr->  resetMode      (MODE_Origin   ); break; //VT100
    case TY_CSI_PR('s',    6) : scr->   saveMode      (MODE_Origin   ); break; //FIXME
    case TY_CSI_PR('r',    6) : scr->restoreMode      (MODE_Origin   ); break; //FIXME
    case TY_CSI_PR('h',    7) : scr->    setMode      (MODE_Wrap     ); break; //VT100
    case TY_CSI_PR('l',    7) : scr->  resetMode      (MODE_Wrap     ); break; //VT100
    case TY_CSI_PR('s',    7) : scr->   saveMode      (MODE_Wrap     ); break; //FIXME
    case TY_CSI_PR('r',    7) : scr->restoreMode      (MODE_Wrap     ); break; //FIXME
    case TY_CSI_PR('h',    8) : /* IGNORED: autorepeat on            */ break; //VT100
    case TY_CSI_PR('l',    8) : /* IGNORED: autorepeat off           */ break; //VT100
    case TY_CSI_PR('h',    9) : /* IGNORED: interlace                */ break; //VT100
    case TY_CSI_PR('l',    9) : /* IGNORED: interlace                */ break; //VT100
    case TY_CSI_PR('h',   25) :          setMode      (MODE_Cursor   ); break; //VT100
    case TY_CSI_PR('l',   25) :        resetMode      (MODE_Cursor   ); break; //VT100
    case TY_CSI_PR('h',   47) :          setMode      (MODE_AppScreen); break; //VT100
    case TY_CSI_PR('l',   47) :        resetMode      (MODE_AppScreen); break; //VT100
    case TY_CSI_PR('h', 1000) :          setMode      (MODE_Mouse1000); break; //XTERM
    case TY_CSI_PR('l', 1000) :        resetMode      (MODE_Mouse1000); break; //XTERM
    case TY_CSI_PR('s', 1000) :         saveMode      (MODE_Mouse1000); break; //XTERM
    case TY_CSI_PR('r', 1000) :      restoreMode      (MODE_Mouse1000); break; //XTERM
    case TY_CSI_PR('h', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM
    case TY_CSI_PR('l', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM
    case TY_CSI_PR('s', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM
    case TY_CSI_PR('r', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM
    case TY_CSI_PR('h', 1047) :          setMode      (MODE_AppScreen); break; //XTERM
    case TY_CSI_PR('l', 1047) :        resetMode      (MODE_AppScreen); break; //XTERM
    case TY_CSI_PR('h', 1048) : scr->saveCursor           (          ); break; //XTERM
    case TY_CSI_PR('l', 1048) : scr->restoreCursor        (          ); break; //XTERM

    //FIXME: when changing between vt52 and ansi mode evtl do some resetting.
    case TY_VT52__('A'      ) : scr->cursorUp             (         1); break; //VT52
    case TY_VT52__('B'      ) : scr->cursorDown           (         1); break; //VT52
    case TY_VT52__('C'      ) : scr->cursorRight          (         1); break; //VT52
    case TY_VT52__('D'      ) : scr->cursorLeft           (         1); break; //VT52
    case TY_VT52__('F'      ) : scr->setAndUseCharset     (0,     '0'); break; //VT52
    case TY_VT52__('G'      ) : scr->setAndUseCharset     (0,     'B'); break; //VT52
    case TY_VT52__('H'      ) : scr->setCursorYX          (1,1       ); break; //VT52
    case TY_VT52__('I'      ) : scr->reverseIndex         (          ); break; //VT52
    case TY_VT52__('J'      ) : scr->clearToEndOfScreen   (          ); break; //VT52
    case TY_VT52__('K'      ) : scr->clearToEndOfLine     (          ); break; //VT52
    case TY_VT52__('Y'      ) : scr->setCursorYX          (p-31,q-31 ); break; //VT52
    case TY_VT52__('Z'      ) :      reportTerminalType   (          ); break; //VT52
    case TY_VT52__('<'      ) :          setMode      (MODE_Ansi     ); break; //VT52
    case TY_VT52__('='      ) :          setMode      (MODE_AppKeyPad); break; //VT52
    case TY_VT52__('>'      ) :        resetMode      (MODE_AppKeyPad); break; //VT52

    default : ReportErrorToken();    break;
  };
}

// -----------------------------------------------------------------------------
//
// Scanner / Transducer
//
// -----------------------------------------------------------------------------

//FIXME: evtl. we have to straiten the scanner a bit.

// Scanning -------------------------------------------------------------------

void VT102Emulation::reset()
{
  ppos = 0; argc = 0; argv[0] = 0; argv[1] = 0;
}

#define CTL  1
#define CHR  2
#define CPN  4
#define DIG  8
#define SCS 16
#define GRP 32

void VT102Emulation::tableInit()
{ int i; UINT8* s;
  for(i =  0;                    i < 256; i++) tbl[ i]  = 0;
  for(i =  0;                    i <  32; i++) tbl[ i] |= CTL;
  for(i = 32;                    i < 256; i++) tbl[ i] |= CHR;
  for(s = (UINT8*)"@ABCDGHLMPXcdfry"; *s; s++) tbl[*s] |= CPN;
  for(s = (UINT8*)"0123456789"      ; *s; s++) tbl[*s] |= DIG;
  for(s = (UINT8*)"()+*"            ; *s; s++) tbl[*s] |= SCS;
  for(s = (UINT8*)"()+*#[]"         ; *s; s++) tbl[*s] |= GRP;
}

/* Ok, here comes the nasty part of the decoder.

   The following defines do not abstract nor explain anything,
   they are only introduced to shorten the lines in the following
   routine, which is their only application.

   - P is the length of the token scanned so far.
   - L (often P-1) is the position on which contents we base a decision.
   - C is a character or a group of characters (taken from 'tbl').
*/

#define lec(P,L,C) (p == (P) &&                     s[(L)]         == (C))
#define les(P,L,C) (p == (P) &&                (tbl[s[(L)]] & (C)) == (C))
#define eec(C)     (p >=  3  &&        cc                          == (C))
#define ees(C)     (p >=  3  &&                (tbl[  cc  ] & (C)) == (C))
#define eps(C)     (p >=  3  && s[2] != '?' && (tbl[  cc  ] & (C)) == (C))
#define epp( )     (p >=  3  && s[2] == '?'                              )
#define Xpe        (ppos>=2  && pbuf[1] == ']'                           )
#define Xte        (Xpe                        &&     cc           ==  7 )
#define ces(C)     (                           (tbl[  cc  ] & (C)) == (C) && !Xte)
#define Dig        a[n] = 10*a[n] + cc - '0';
#define Arg        argc = MIN(argc+1,MAXARGS-1); argv[argc] = 0;

//FIXME: introduce real states.
//
//

void VT102Emulation::onRcvByte(int c)
{ unsigned char cc = c; int i;
  if (cc == 127) return; //VT100: ignore.

  if (ces(    CTL))
  {
    if (cc == CNTL('X') || cc == CNTL('Z') || cc == ESC) reset(); //VT100: CAN or SUB
    if (cc != ESC)    { tau( TY_CTL___(cc     ),    0,   0);          return; }
  }
  pbuf[ppos] = cc; ppos = MIN(ppos+1,MAXPBUF-1);
  unsigned char* s = pbuf;
  int            p = ppos;
  int *          a = argv;
  int            n = argc;
  if (getMode(MODE_Ansi))
  {
    if (lec(1,0,ESC)) {                                               return; }
    if (les(2,1,GRP)) {                                               return; }
    if (Xte         ) { XtermHack();                         reset(); return; }
    if (Xpe         ) {                                               return; }
    if (lec(3,2,'?')) {                                               return; }
    if (les(1,0,CHR)) { tau( TY_CHR___(       ), s[0],   0); reset(); return; }
    if (lec(2,0,ESC)) { tau( TY_ESC___(s[1]   ),    0,   0); reset(); return; }
    if (les(3,1,SCS)) { tau( TY_ESC_CS(       ), s[1],s[2]); reset(); return; }
    if (lec(3,1,'#')) { tau( TY_ESC_DE(s[2]   ),    0,   0); reset(); return; }
    if (eps(    CPN)) { tau( TY_CSI_PN(cc     ), a[0],a[1]); reset(); return; }
    if (ees(    DIG)) { Dig                                           return; }
    if (eec(    ';')) { Arg                                           return; }
    for (i=0;i<=n;i++)
    if (epp(       ))   tau( TY_CSI_PR(cc,a[i]),    0,   0);          else
                        tau( TY_CSI_PS(cc,a[i]),    0,   0);
    reset();
  }
  else // mode VT52
  {
    if (lec(1,0,ESC))                                                 return;
    if (les(1,0,CHR)) { tau( TY_CHR___(       ), s[0],   0); reset(); return; }
    if (lec(2,1,'Y'))                                                 return;
    if (lec(3,1,'Y'))                                                 return;
    if (p < 4)        { tau( TY_VT52__(s[1]   ),    0,   0); reset(); return; }
                        tau( TY_VT52__(s[1]   ), s[2],s[3]); reset(); return;
  }
}

void VT102Emulation::XtermHack()
{ int i,arg = 0;
  for (i = 2; i < ppos && '0'<=pbuf[i] && pbuf[i]<'9' ; i++)
    arg = 10*arg + (pbuf[i]-'0');
  if (pbuf[i] != ';') { ReportErrorToken(); return; }
  char *str = new char[ppos-i-1];
  strncpy(str,(char*)pbuf+i+1,ppos-i-2);
  str[ppos-i-2]='\0';
  if (arg <= 2) emit changeTitle(arg,str);
  delete str;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               Reporting                                   */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*! shows the contents of the scan buffer.

    This functions is used for diagnostics. It is called by \e ReportErrorToken
    to inform about strings that cannot be decoded or handled by the emulation.

    \sa ReportErrorToken
*/

/*!
*/

static void hexdump(char* s, int len)
{ int i;
  for (i = 0; i < len; i++)
  {
    if (s[i] == '\\')
      printf("\\\\");
    else
    if ((s[i]&0xff) > 32)
      printf("%c",s[i]);
    else
      printf("\\%02x",s[i]&0xff);
  }
}

void VT102Emulation::scan_buffer_report()
{
  if (ppos == 0 || ppos == 1 && (pbuf[0] & 0xff) >= 32) return;
  printf("token: "); hexdump((char*)pbuf,ppos); printf("\n");
}

/*!
*/

void VT102Emulation::ReportErrorToken()
{
  printf("undecodable "); scan_buffer_report();
}

void VT102Emulation::NotImplemented(char* text)
{
  printf("not implemented: %s.",text); scan_buffer_report();
}

/*!
*/

void VT102Emulation::sendString(const char* s)
{
  emit sndBlock(s,strlen(s));
}

/*!
*/

void VT102Emulation::reportTerminalType()
{
//FIXME: should change?
  if (getMode(MODE_Ansi))
    sendString("\033[?1;2c"); // I'm a VT100 with AP0
  else
    sendString("\033/Z");     // I'm a VT52
}

/*!
*/

void VT102Emulation::reportStatus()
{
  sendString("\033[0n"); //VT100. Device status report. 0 = Ready.
}

/*!
*/

void VT102Emulation::reportAnswerBack()
{
  sendString("konsole"); //FIXME? make configurable?
}

/*!
*/

void VT102Emulation::reportCursorPosition()
{ char tmp[20];
  sprintf(tmp,"\033[%d;%dR",scr->getCursorY()+1,scr->getCursorX()+1);
  sendString(tmp);
}

/*!
    `x',`y' are 1-based.
    `ev' (event) indicates the button pressed (0-2)
                 or a general mouse release (3).
*/

void VT102Emulation::reportMouseEvent(int ev, int x, int y)
{ char tmp[20];
  sprintf(tmp,"\033[M%c%c%c",ev+040,x+040,y+040);
  sendString(tmp);
}

void VT102Emulation::reportTerminalParms(int p)
// DECREPTPARM
{ char tmp[100];
  sprintf(tmp,"\033[%d;1;1;112;112;1;0x",p); // not really true.
  sendString(tmp);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Mode Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

// NOTE: experimental section.
//       This is due to the fact that modes have to be handled both on
//       emulation and screen level. Dought that we can clean this up.

void VT102Emulation::setMode(int m)
{
  currParm.mode[m] = TRUE;
  switch (m)
  {
    case MODE_Mouse1000 : gui->setMouseMarks(FALSE);
    break;
    case MODE_AppScreen : screen[1]->clearSelection();
                          screen[1]->clearEntireScreen();
                          setScreen(1);
	  break;
  }
  if (m < MODES_SCREEN || m == MODE_NewLine)
  {
    screen[0]->setMode(m);
    screen[1]->setMode(m);
  }
}

void VT102Emulation::resetMode(int m)
{
  currParm.mode[m] = FALSE;
  switch (m)
  {
    case MODE_Mouse1000 : gui->setMouseMarks(TRUE);
    break;
    case MODE_AppScreen : screen[0]->clearSelection();
                          setScreen(0);
    break;
  }
  if (m < MODES_SCREEN || m == MODE_NewLine)
  {
    screen[0]->resetMode(m);
    screen[1]->resetMode(m);
  }
}

void VT102Emulation::saveMode(int m)
{
  saveParm.mode[m] = currParm.mode[m];
}

void VT102Emulation::restoreMode(int m)
{
  if(saveParm.mode[m]) setMode(m); else resetMode(m);
}

//NOTE: this is a helper function
BOOL VT102Emulation::getMode(int m)
{
  return currParm.mode[m];
}

void VT102Emulation::setConnect(bool c)
{
  Emulation::setConnect(c);
  if (c)
  { // refresh mouse mode
    if (getMode(MODE_Mouse1000))
      setMode(MODE_Mouse1000);
    else
      resetMode(MODE_Mouse1000);
  }
}

void VT102Emulation::setCharset(int n, int cs)
{
  screen[0]->setCharset(n, cs);
  screen[1]->setCharset(n, cs);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Miscelaneous                               */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*! change between primary and alternate screen
*/

void VT102Emulation::setScreen(int n)
{
  scr = screen[n&1];
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Mouse Handling                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void VT102Emulation::onMouse( int cb, int cx, int cy )
{
  if (!connected) return;
  reportMouseEvent(cb,cx,cy);
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                             Keyboard Handling                             */
/*                                                                           */
/* ------------------------------------------------------------------------- */

#define KeyComb(B,K) ((ev->state() & (B)) == (B) && ev->key() == (K))

#define Xterm (!strcmp(emulation.data(),"xterm"))
/*!
*/

void VT102Emulation::onKeyPress( QKeyEvent* ev )
{ int key;

  if (!connected) return; // someone else gets the keys

  // revert to non-history when typing
  if (scr->getHistCursor() != scr->getHistLines());
    scr->setHistCursor(scr->getHistLines());

  // Note: there 3 ways in rxvt to handle the Meta (Alt) key
  //       1) ignore it
  //       2) preceed the keycode by ESC (what we do here)
  //       3) set the 8th bit of each char in string
  //          (which may fail for 8bit (european) characters.
  if (ev->state() & AltButton) sendString("\033"); // ESC

//printf("State/Key: 0x%04x 0x%04x\n",ev->state(),ev->key());

  key = ev->key();
  switch (key)
  {
    case Key_Return    : sendString(getMode(MODE_NewLine)?"\r\n"   :"\r"  ); return;
    case Key_Backspace : sendString(getMode(MODE_BsHack )?"\x7f"   :"\x08"); return;
    case Key_Delete    : sendString(getMode(MODE_BsHack )?"\033[3~":"\x7f"); return;

    case Key_Up        : sendString(!getMode(MODE_Ansi)?"\033A":getMode(MODE_AppCuKeys)?"\033OA":"\033[A"); return;
    case Key_Down      : sendString(!getMode(MODE_Ansi)?"\033B":getMode(MODE_AppCuKeys)?"\033OB":"\033[B"); return;
    case Key_Right     : sendString(!getMode(MODE_Ansi)?"\033C":getMode(MODE_AppCuKeys)?"\033OC":"\033[C"); return;
    case Key_Left      : sendString(!getMode(MODE_Ansi)?"\033D":getMode(MODE_AppCuKeys)?"\033OD":"\033[D"); return;

                                    //      XTERM      LINUX
    case Key_F1        : sendString(Xterm? "\033[11~": "\033[[A" ); return;
    case Key_F2        : sendString(Xterm? "\033[12~": "\033[[B" ); return;
    case Key_F3        : sendString(Xterm? "\033[13~": "\033[[C" ); return;
    case Key_F4        : sendString(Xterm? "\033[14~": "\033[[D" ); return;
    case Key_F5        : sendString(Xterm? "\033[15~": "\033[[E" ); return;
    case Key_F6        : sendString("\033[17~" ); return;
    case Key_F7        : sendString("\033[18~" ); return;
    case Key_F8        : sendString("\033[19~" ); return;
    case Key_F9        : sendString("\033[20~" ); return;
    case Key_F10       : sendString("\033[21~" ); return;
    case Key_F11       : sendString("\033[23~" ); return;
    case Key_F12       : sendString("\033[24~" ); return;

    case Key_Home      : sendString("\033[H"  ); return;
    case Key_End       : sendString("\033[F"  ); return;
    case Key_Prior     : sendString("\033[5~"  ); return;
    case Key_Next      : sendString("\033[6~"  ); return;
    case Key_Insert    : sendString("\033[2~"  ); return;
    //FIXME: get keypad somehow
  }
  if (KeyComb(ControlButton,Key_Space)) // ctrl-Space == ctrl-@
  {
    sndBlock("\x00",1); return;
  }
  if (KeyComb(ControlButton,Key_Print)) // ctrl-print == sys-req
  {
    reportAnswerBack(); return;
  }
  if (ev->ascii()>0)
  { unsigned char c[1];
    c[0] = ev->ascii();
//printf("ansi key: "); hexdump(c,1); printf("\n");
    emit sndBlock((char*)c,1);
    return;
  }
}
