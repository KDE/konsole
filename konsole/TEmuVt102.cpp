/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

/*! \class TEmuVt102

   \brief Actual Emulation for Konsole

   \sa TEWidget \sa TEScreen
*/
#include "config-konsole.h"


#if defined(__osf__) || defined(__APPLE__)
#define AVOID_XKB
#endif


// this allows konsole to be compiled without XKB and XTEST extensions
// even though it might be available on a particular system.
#if defined(AVOID_XKB)
#undef HAVE_XKB
#endif

// Standard 
#include <stdio.h>
#include <unistd.h>
#include <assert.h>

// Qt
#include <QKeyEvent>
#include <QByteArray>

// KDE
#include <kdebug.h>
#include <klocale.h>

// Konsole
#include "TEmuVt102.h"
#include "TEScreen.h"

/* VT102 Terminal Emulation

   This class puts together the screens, the pty and the widget to a
   complete terminal emulation. Beside combining it's componentes, it
   handles the emulations's protocol.

   This module consists of the following sections:

   - Constructor/Destructor
   - Incoming Bytes Event pipeline
   - Outgoing Bytes
     - Mouse Events
     - Keyboard Events
   - Modes and Charset State
   - Diagnostics
*/

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                       Constructor / Destructor                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*
   Nothing really intesting happens here.
*/

/*!
*/

TEmuVt102::TEmuVt102() : TEmulation()
{
  titleUpdateTimer.setSingleShot(true);

  QObject::connect(&titleUpdateTimer , SIGNAL(timeout()) , this , SLOT(updateTitle()));

  initTokenizer();
  reset();
}

#if 0
void TEmuVt102::setReceiveViewInput(TEWidget* view , bool enable)
{
   if (enable)
   {
        QObject::connect(view,SIGNAL(mouseSignal(int,int,int,int)),
                  this,SLOT(onMouse(int,int,int,int)));
        QObject::connect(view, SIGNAL(sendStringToEmu(const char*)),
 		   this, SLOT(sendString(const char*)));
   }
   else
   {
        QObject::disconnect(view,0,
                  this,SLOT(onMouse(int,int,int,int)));
        QObject::disconnect(view,0,
 		   this, SLOT(sendString(const char*)));
   }
}
#endif

#if 0
void TEmuVt102::addView(TEWidget* view)
{
    TEmulation::addView(view);
    setReceiveViewInput(view,true);
}

void TEmuVt102::removeView(TEWidget* view)
{
    TEmulation::removeView(view);
    setReceiveViewInput(view,false);
}
#endif 


/*!
*/

TEmuVt102::~TEmuVt102()
{
}

/*!
*/

void TEmuVt102::clearEntireScreen()
{
  currentScreen->clearEntireScreen();

  bufferedUpdate(); 
}

void TEmuVt102::reset()
{
  //kDebug(1211)<<"TEmuVt102::reset() resetToken()"<<endl;
  resetToken();
  //kDebug(1211)<<"TEmuVt102::reset() resetModes()"<<endl;
  resetModes();
  //kDebug(1211)<<"TEmuVt102::reset() resetCharSet()"<<endl;
  resetCharset(0);
  //kDebug(1211)<<"TEmuVt102::reset() reset screen0()"<<endl;
  screen[0]->reset();
  //kDebug(1211)<<"TEmuVt102::reset() resetCharSet()"<<endl;
  resetCharset(1);
  //kDebug(1211)<<"TEmuVt102::reset() reset screen 1"<<endl;
  screen[1]->reset();
  //kDebug(1211)<<"TEmuVt102::reset() setCodec()"<<endl;
  setCodec(0);
  //kDebug(1211)<<"TEmuVt102::reset() done"<<endl;
 
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
   `TEScreen' class or by the emulation class itself.

   The pipeline proceeds as follows:

   - Tokenizing the ESC codes (onReceiveChar)
   - VT100 code page translation of plain characters (applyCharset)
   - Interpretation of ESC codes (tau)

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

#define TY_CHR(   )     TY_CONSTR(0,0,0)
#define TY_CTL(A  )     TY_CONSTR(1,A,0)
#define TY_ESC(A  )     TY_CONSTR(2,A,0)
#define TY_ESC_CS(A,B)  TY_CONSTR(3,A,B)
#define TY_ESC_DE(A  )  TY_CONSTR(4,A,0)
#define TY_CSI_PS(A,N)  TY_CONSTR(5,A,N)
#define TY_CSI_PN(A  )  TY_CONSTR(6,A,0)
#define TY_CSI_PR(A,N)  TY_CONSTR(7,A,N)

#define TY_VT52(A  )    TY_CONSTR(8,A,0)

#define TY_CSI_PG(A  )  TY_CONSTR(9,A,0)

#define TY_CSI_PE(A  )  TY_CONSTR(10,A,0)

// Tokenizer --------------------------------------------------------------- --

/* The tokenizers state

   The state is represented by the buffer (pbuf, ppos),
   and accompanied by decoded arguments kept in (argv,argc).
   Note that they are kept internal in the tokenizer.
*/

void TEmuVt102::resetToken()
{
  ppos = 0; argc = 0; argv[0] = 0; argv[1] = 0;
}

void TEmuVt102::addDigit(int dig)
{
  argv[argc] = 10*argv[argc] + dig;
}

void TEmuVt102::addArgument()
{
  argc = qMin(argc+1,MAXARGS-1);
  argv[argc] = 0;
}

void TEmuVt102::pushToToken(int cc)
{
  pbuf[ppos] = cc;
  ppos = qMin(ppos+1,MAXPBUF-1);
}

// Character Classes used while decoding

#define CTL  1
#define CHR  2
#define CPN  4
#define DIG  8
#define SCS 16
#define GRP 32
#define CPS 64

void TEmuVt102::initTokenizer()
{ int i; UINT8* s;
  for(i =  0;                      i < 256; i++) tbl[ i]  = 0;
  for(i =  0;                      i <  32; i++) tbl[ i] |= CTL;
  for(i = 32;                      i < 256; i++) tbl[ i] |= CHR;
  for(s = (UINT8*)"@ABCDGHILMPSTXZcdfry"; *s; s++) tbl[*s] |= CPN;
// resize = \e[8;<row>;<col>t
  for(s = (UINT8*)"t"; *s; s++) tbl[*s] |= CPS;
  for(s = (UINT8*)"0123456789"        ; *s; s++) tbl[*s] |= DIG;
  for(s = (UINT8*)"()+*%"             ; *s; s++) tbl[*s] |= SCS;
  for(s = (UINT8*)"()+*#[]%"          ; *s; s++) tbl[*s] |= GRP;
  resetToken();
}

/* Ok, here comes the nasty part of the decoder.

   Instead of keeping an explicit state, we deduce it from the
   token scanned so far. It is then immediately combined with
   the current character to form a scanning decision.

   This is done by the following defines.

   - P is the length of the token scanned so far.
   - L (often P-1) is the position on which contents we base a decision.
   - C is a character or a group of characters (taken from 'tbl').

   Note that they need to applied in proper order.
*/

#define lec(P,L,C) (p == (P) &&                     s[(L)]         == (C))
#define lun(     ) (p ==  1  &&                       cc           >= 32 )
#define les(P,L,C) (p == (P) && s[L] < 256  && (tbl[s[(L)]] & (C)) == (C))
#define eec(C)     (p >=  3  &&        cc                          == (C))
#define ees(C)     (p >=  3  && cc < 256 &&    (tbl[  cc  ] & (C)) == (C))
#define eps(C)     (p >=  3  && s[2] != '?' && s[2] != '!' && s[2] != '>' && cc < 256 && (tbl[  cc  ] & (C)) == (C))
#define epp( )     (p >=  3  && s[2] == '?'                              )
#define epe( )     (p >=  3  && s[2] == '!'                              )
#define egt(     ) (p >=  3  && s[2] == '>'                              )
#define Xpe        (ppos>=2  && pbuf[1] == ']'                           )
#define Xte        (Xpe                        &&     cc           ==  7 )
#define ces(C)     (            cc < 256 &&    (tbl[  cc  ] & (C)) == (C) && !Xte)

#define ESC 27
#define CNTL(c) ((c)-'@')

// process an incoming unicode character

void TEmuVt102::onReceiveChar(int cc)
{ 
  int i;
  if (cc == 127) return; //VT100: ignore.

  if (ces(    CTL))
  { // DEC HACK ALERT! Control Characters are allowed *within* esc sequences in VT100
    // This means, they do neither a resetToken nor a pushToToken. Some of them, do
    // of course. Guess this originates from a weakly layered handling of the X-on
    // X-off protocol, which comes really below this level.
    if (cc == CNTL('X') || cc == CNTL('Z') || cc == ESC) resetToken(); //VT100: CAN or SUB
    if (cc != ESC)    { tau( TY_CTL(cc+'@' ),   0,  0); return; }
  }

  pushToToken(cc); // advance the state

  int* s = pbuf;
  int  p = ppos;

  if (getMode(MODE_Ansi)) // decide on proper action
  {
    if (lec(1,0,ESC)) {                                                       return; }
    if (lec(1,0,ESC+128)) { s[0] = ESC; onReceiveChar('[');                   return; }
    if (les(2,1,GRP)) {                                                       return; }
    if (Xte         ) { XtermHack();                            resetToken(); return; }
    if (Xpe         ) {                                                       return; }
    if (lec(3,2,'?')) {                                                       return; }
    if (lec(3,2,'>')) {                                                       return; }
    if (lec(3,2,'!')) {                                                       return; }
    if (lun(       )) { tau( TY_CHR(), applyCharset(cc), 0); resetToken(); return; }
    if (lec(2,0,ESC)) { tau( TY_ESC(s[1]),   0,  0);       resetToken(); return; }
    if (les(3,1,SCS)) { tau( TY_ESC_CS(s[1],s[2]),   0,  0);  resetToken(); return; }
    if (lec(3,1,'#')) { tau( TY_ESC_DE(s[2]),   0,  0);       resetToken(); return; }
    if (eps(    CPN)) { tau( TY_CSI_PN(cc), argv[0],argv[1]);   resetToken(); return; }

// resize = \e[8;<row>;<col>t
    if (eps(    CPS)) { tau( TY_CSI_PS(cc, argv[0]), argv[1], argv[2]);   resetToken(); return; }

    if (epe(       )) { tau( TY_CSI_PE(cc),     0,  0);       resetToken(); return; }
    if (ees(    DIG)) { addDigit(cc-'0');                                     return; }
    if (eec(    ';')) { addArgument();                                        return; }
    for (i=0;i<=argc;i++)
    if ( epp(     ))  { tau( TY_CSI_PR(cc,argv[i]),   0,  0); }
    else if(egt(    ))   { tau( TY_CSI_PG(cc     ),   0,  0); } // spec. case for ESC]>0c or ESC]>c
    else if (cc == 'm' && argc - i >= 4 && (argv[i] == 38 || argv[i] == 48) && argv[i+1] == 2)
    { // ESC[ ... 48;2;<red>;<green>;<blue> ... m -or- ESC[ ... 38;2;<red>;<green>;<blue> ... m
      i += 2;
      tau( TY_CSI_PS(cc, argv[i-2]), COLOR_SPACE_RGB, (argv[i] << 16) | (argv[i+1] << 8) | argv[i+2]);
      i += 2;
    }
    else if (cc == 'm' && argc - i >= 2 && (argv[i] == 38 || argv[i] == 48) && argv[i+1] == 5)
    { // ESC[ ... 48;5;<index> ... m -or- ESC[ ... 38;5;<index> ... m
      i += 2;
      tau( TY_CSI_PS(cc, argv[i-2]), COLOR_SPACE_256, argv[i]);
    }
    else              { tau( TY_CSI_PS(cc,argv[i]),   0,  0); }
    resetToken();
  }
  else // mode VT52
  {
    if (lec(1,0,ESC))                                                      return;
    if (les(1,0,CHR)) { tau( TY_CHR(       ), s[0],  0); resetToken(); return; }
    if (lec(2,1,'Y'))                                                      return;
    if (lec(3,1,'Y'))                                                      return;
    if (p < 4)        { tau( TY_VT52(s[1]   ),   0,  0); resetToken(); return; }
                        tau( TY_VT52(s[1]   ), s[2],s[3]); resetToken(); return;
  }
}

void TEmuVt102::XtermHack()
{ int i,arg = 0;
  for (i = 2; i < ppos && '0'<=pbuf[i] && pbuf[i]<'9' ; i++)
    arg = 10*arg + (pbuf[i]-'0');
  if (pbuf[i] != ';') { ReportErrorToken(); return; }
  QChar *str = new QChar[ppos-i-2];
  for (int j = 0; j < ppos-i-2; j++) str[j] = pbuf[i+1+j];
  QString unistr(str,ppos-i-2);
  // arg == 1 doesn't change the title. In XTerm it only changes the icon name
  // (btw: arg=0 changes title and icon, arg=1 only icon, arg=2 only title
//  emit changeTitle(arg,unistr);
  pendingTitleUpdates[arg] = unistr;
  titleUpdateTimer.start(20);

  delete [] str;
}

void TEmuVt102::updateTitle()
{
	QListIterator<int> iter( pendingTitleUpdates.keys() );
	while (iter.hasNext()) {
		int arg = iter.next();
		emit changeTitle( arg , pendingTitleUpdates[arg] );	
	}

    pendingTitleUpdates.clear();	
}

// Interpreting Codes ---------------------------------------------------------

/*
   Now that the incoming character stream is properly tokenized,
   meaning is assigned to them. These are either operations of
   the current screen, or of the emulation class itself.

   The token to be interpreteted comes in as a machine word
   possibly accompanied by two parameters.

   Likewise, the operations assigned to, come with up to two
   arguments. One could consider to make up a proper table
   from the function below.

   The technical reference manual provides more information
   about this mapping.
*/

void TEmuVt102::tau( int token, int p, int q )
{
#if 0
int N = (token>>0)&0xff;
int A = (token>>8)&0xff;
switch( N )
{
   case 0: printf("%c", (p < 128) ? p : '?');
           break;
   case 1: if (A == 'J') printf("\r");
           else if (A == 'M') printf("\n");
           else printf("CTL-%c ", (token>>8)&0xff);
           break;
   case 2: printf("ESC-%c ", (token>>8)&0xff);
           break;
   case 3: printf("ESC_CS-%c-%c ", (token>>8)&0xff, (token>>16)&0xff);
           break;
   case 4: printf("ESC_DE-%c ", (token>>8)&0xff);
           break;
   case 5: printf("CSI-PS-%c-%d", (token>>8)&0xff, (token>>16)&0xff );
           break;
   case 6: printf("CSI-PN-%c [%d]", (token>>8)&0xff, p);
           break;
   case 7: printf("CSI-PR-%c-%d", (token>>8)&0xff, (token>>16)&0xff );
           break;
   case 8: printf("VT52-%c", (token>>8)&0xff);
           break;
   case 9: printf("CSI-PG-%c", (token>>8)&0xff);
           break;
   case 10: printf("CSI-PE-%c", (token>>8)&0xff);
           break;
}
#endif

  switch (token)
  {

    case TY_CHR(         ) : currentScreen->ShowCharacter        (p         ); break; //UTF16

    //             127 DEL    : ignored on input

    case TY_CTL('@'      ) : /* NUL: ignored                      */ break;
    case TY_CTL('A'      ) : /* SOH: ignored                      */ break;
    case TY_CTL('B'      ) : /* STX: ignored                      */ break;
    case TY_CTL('C'      ) : /* ETX: ignored                      */ break;
    case TY_CTL('D'      ) : /* EOT: ignored                      */ break;
    case TY_CTL('E'      ) :      reportAnswerBack     (          ); break; //VT100
    case TY_CTL('F'      ) : /* ACK: ignored                      */ break;
    case TY_CTL('G'      ) : emit notifySessionState(NOTIFYBELL);
                                break; //VT100
    case TY_CTL('H'      ) : currentScreen->BackSpace            (          ); break; //VT100
    case TY_CTL('I'      ) : currentScreen->Tabulate             (          ); break; //VT100
    case TY_CTL('J'      ) : currentScreen->NewLine              (          ); break; //VT100
    case TY_CTL('K'      ) : currentScreen->NewLine              (          ); break; //VT100
    case TY_CTL('L'      ) : currentScreen->NewLine              (          ); break; //VT100
    case TY_CTL('M'      ) : currentScreen->Return               (          ); break; //VT100

    case TY_CTL('N'      ) :      useCharset           (         1); break; //VT100
    case TY_CTL('O'      ) :      useCharset           (         0); break; //VT100

    case TY_CTL('P'      ) : /* DLE: ignored                      */ break;
    case TY_CTL('Q'      ) : /* DC1: XON continue                 */ break; //VT100
    case TY_CTL('R'      ) : /* DC2: ignored                      */ break;
    case TY_CTL('S'      ) : /* DC3: XOFF halt                    */ break; //VT100
    case TY_CTL('T'      ) : /* DC4: ignored                      */ break;
    case TY_CTL('U'      ) : /* NAK: ignored                      */ break;
    case TY_CTL('V'      ) : /* SYN: ignored                      */ break;
    case TY_CTL('W'      ) : /* ETB: ignored                      */ break;
    case TY_CTL('X'      ) : currentScreen->ShowCharacter        (    0x2592); break; //VT100
    case TY_CTL('Y'      ) : /* EM : ignored                      */ break;
    case TY_CTL('Z'      ) : currentScreen->ShowCharacter        (    0x2592); break; //VT100
    case TY_CTL('['      ) : /* ESC: cannot be seen here.         */ break;
    case TY_CTL('\\'     ) : /* FS : ignored                      */ break;
    case TY_CTL(']'      ) : /* GS : ignored                      */ break;
    case TY_CTL('^'      ) : /* RS : ignored                      */ break;
    case TY_CTL('_'      ) : /* US : ignored                      */ break;

    case TY_ESC('D'      ) : currentScreen->index                (          ); break; //VT100
    case TY_ESC('E'      ) : currentScreen->NextLine             (          ); break; //VT100
    case TY_ESC('H'      ) : currentScreen->changeTabStop        (true      ); break; //VT100
    case TY_ESC('M'      ) : currentScreen->reverseIndex         (          ); break; //VT100
    case TY_ESC('Z'      ) :      reportTerminalType   (          ); break;
    case TY_ESC('c'      ) :      reset                (          ); break;

    case TY_ESC('n'      ) :      useCharset           (         2); break;
    case TY_ESC('o'      ) :      useCharset           (         3); break;
    case TY_ESC('7'      ) :      saveCursor           (          ); break;
    case TY_ESC('8'      ) :      restoreCursor        (          ); break;

    case TY_ESC('='      ) :          setMode      (MODE_AppKeyPad); break;
    case TY_ESC('>'      ) :        resetMode      (MODE_AppKeyPad); break;
    case TY_ESC('<'      ) :          setMode      (MODE_Ansi     ); break; //VT100

    case TY_ESC_CS('(', '0') :      setCharset           (0,    '0'); break; //VT100
    case TY_ESC_CS('(', 'A') :      setCharset           (0,    'A'); break; //VT100
    case TY_ESC_CS('(', 'B') :      setCharset           (0,    'B'); break; //VT100

    case TY_ESC_CS(')', '0') :      setCharset           (1,    '0'); break; //VT100
    case TY_ESC_CS(')', 'A') :      setCharset           (1,    'A'); break; //VT100
    case TY_ESC_CS(')', 'B') :      setCharset           (1,    'B'); break; //VT100

    case TY_ESC_CS('*', '0') :      setCharset           (2,    '0'); break; //VT100
    case TY_ESC_CS('*', 'A') :      setCharset           (2,    'A'); break; //VT100
    case TY_ESC_CS('*', 'B') :      setCharset           (2,    'B'); break; //VT100

    case TY_ESC_CS('+', '0') :      setCharset           (3,    '0'); break; //VT100
    case TY_ESC_CS('+', 'A') :      setCharset           (3,    'A'); break; //VT100
    case TY_ESC_CS('+', 'B') :      setCharset           (3,    'B'); break; //VT100

    case TY_ESC_CS('%', 'G') :      setCodec             (1         ); break; //LINUX
    case TY_ESC_CS('%', '@') :      setCodec             (0         ); break; //LINUX

    case TY_ESC_DE('3'      ) : /* Double height line, top half    */ 
								currentScreen->setLineProperty( LINE_DOUBLEWIDTH , true );
								currentScreen->setLineProperty( LINE_DOUBLEHEIGHT , true );
									break;
    case TY_ESC_DE('4'      ) : /* Double height line, bottom half */ 
								currentScreen->setLineProperty( LINE_DOUBLEWIDTH , true );
								currentScreen->setLineProperty( LINE_DOUBLEHEIGHT , true );
									break;
    case TY_ESC_DE('5'      ) : /* Single width, single height line*/
								currentScreen->setLineProperty( LINE_DOUBLEWIDTH , false);
								currentScreen->setLineProperty( LINE_DOUBLEHEIGHT , false);
								break;
    case TY_ESC_DE('6'      ) : /* Double width, single height line*/ 
							    currentScreen->setLineProperty( LINE_DOUBLEWIDTH , true);	
								currentScreen->setLineProperty( LINE_DOUBLEHEIGHT , false);
								break;
    case TY_ESC_DE('8'      ) : currentScreen->helpAlign            (          ); break;

// resize = \e[8;<row>;<col>t
    case TY_CSI_PS('t',   8) : setScreenSize( q /* colums */, p /* lines */ );    break;

// change tab text color : \e[28;<color>t  color: 0-16,777,215
    case TY_CSI_PS('t',   28) : emit changeTabTextColor   ( p        );          break;

    case TY_CSI_PS('K',   0) : currentScreen->clearToEndOfLine     (          ); break;
    case TY_CSI_PS('K',   1) : currentScreen->clearToBeginOfLine   (          ); break;
    case TY_CSI_PS('K',   2) : currentScreen->clearEntireLine      (          ); break;
    case TY_CSI_PS('J',   0) : currentScreen->clearToEndOfScreen   (          ); break;
    case TY_CSI_PS('J',   1) : currentScreen->clearToBeginOfScreen (          ); break;
    case TY_CSI_PS('J',   2) : currentScreen->clearEntireScreen    (          ); break;
    case TY_CSI_PS('g',   0) : currentScreen->changeTabStop        (false     ); break; //VT100
    case TY_CSI_PS('g',   3) : currentScreen->clearTabStops        (          ); break; //VT100
    case TY_CSI_PS('h',   4) : currentScreen->    setMode      (MODE_Insert   ); break;
    case TY_CSI_PS('h',  20) :          setMode      (MODE_NewLine  ); break;
    case TY_CSI_PS('i',   0) : /* IGNORE: attached printer          */ break; //VT100
    case TY_CSI_PS('l',   4) : currentScreen->  resetMode      (MODE_Insert   ); break;
    case TY_CSI_PS('l',  20) :        resetMode      (MODE_NewLine  ); break;
    case TY_CSI_PS('s',   0) :      saveCursor           (          ); break;
    case TY_CSI_PS('u',   0) :      restoreCursor        (          ); break;

    case TY_CSI_PS('m',   0) : currentScreen->setDefaultRendition  (          ); break;
    case TY_CSI_PS('m',   1) : currentScreen->  setRendition     (RE_BOLD     ); break; //VT100
    case TY_CSI_PS('m',   4) : currentScreen->  setRendition     (RE_UNDERLINE); break; //VT100
    case TY_CSI_PS('m',   5) : currentScreen->  setRendition     (RE_BLINK    ); break; //VT100
    case TY_CSI_PS('m',   7) : currentScreen->  setRendition     (RE_REVERSE  ); break;
    case TY_CSI_PS('m',  10) : /* IGNORED: mapping related          */ break; //LINUX
    case TY_CSI_PS('m',  11) : /* IGNORED: mapping related          */ break; //LINUX
    case TY_CSI_PS('m',  12) : /* IGNORED: mapping related          */ break; //LINUX
    case TY_CSI_PS('m',  22) : currentScreen->resetRendition     (RE_BOLD     ); break;
    case TY_CSI_PS('m',  24) : currentScreen->resetRendition     (RE_UNDERLINE); break;
    case TY_CSI_PS('m',  25) : currentScreen->resetRendition     (RE_BLINK    ); break;
    case TY_CSI_PS('m',  27) : currentScreen->resetRendition     (RE_REVERSE  ); break;

    case TY_CSI_PS('m',   30) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  0); break;
    case TY_CSI_PS('m',   31) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  1); break;
    case TY_CSI_PS('m',   32) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  2); break;
    case TY_CSI_PS('m',   33) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  3); break;
    case TY_CSI_PS('m',   34) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  4); break;
    case TY_CSI_PS('m',   35) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  5); break;
    case TY_CSI_PS('m',   36) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  6); break;
    case TY_CSI_PS('m',   37) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  7); break;

    case TY_CSI_PS('m',   38) : currentScreen->setForeColor         (p,       q); break;

    case TY_CSI_PS('m',   39) : currentScreen->setForeColor         (COLOR_SPACE_DEFAULT,  0); break;

    case TY_CSI_PS('m',   40) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  0); break;
    case TY_CSI_PS('m',   41) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  1); break;
    case TY_CSI_PS('m',   42) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  2); break;
    case TY_CSI_PS('m',   43) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  3); break;
    case TY_CSI_PS('m',   44) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  4); break;
    case TY_CSI_PS('m',   45) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  5); break;
    case TY_CSI_PS('m',   46) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  6); break;
    case TY_CSI_PS('m',   47) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  7); break;

    case TY_CSI_PS('m',   48) : currentScreen->setBackColor         (p,       q); break;

    case TY_CSI_PS('m',   49) : currentScreen->setBackColor         (COLOR_SPACE_DEFAULT,  1); break;

    case TY_CSI_PS('m',   90) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  8); break;
    case TY_CSI_PS('m',   91) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM,  9); break;
    case TY_CSI_PS('m',   92) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 10); break;
    case TY_CSI_PS('m',   93) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 11); break;
    case TY_CSI_PS('m',   94) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 12); break;
    case TY_CSI_PS('m',   95) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 13); break;
    case TY_CSI_PS('m',   96) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 14); break;
    case TY_CSI_PS('m',   97) : currentScreen->setForeColor         (COLOR_SPACE_SYSTEM, 15); break;

    case TY_CSI_PS('m',  100) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  8); break;
    case TY_CSI_PS('m',  101) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM,  9); break;
    case TY_CSI_PS('m',  102) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 10); break;
    case TY_CSI_PS('m',  103) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 11); break;
    case TY_CSI_PS('m',  104) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 12); break;
    case TY_CSI_PS('m',  105) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 13); break;
    case TY_CSI_PS('m',  106) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 14); break;
    case TY_CSI_PS('m',  107) : currentScreen->setBackColor         (COLOR_SPACE_SYSTEM, 15); break;

    case TY_CSI_PS('n',   5) :      reportStatus         (          ); break;
    case TY_CSI_PS('n',   6) :      reportCursorPosition (          ); break;
    case TY_CSI_PS('q',   0) : /* IGNORED: LEDs off                 */ break; //VT100
    case TY_CSI_PS('q',   1) : /* IGNORED: LED1 on                  */ break; //VT100
    case TY_CSI_PS('q',   2) : /* IGNORED: LED2 on                  */ break; //VT100
    case TY_CSI_PS('q',   3) : /* IGNORED: LED3 on                  */ break; //VT100
    case TY_CSI_PS('q',   4) : /* IGNORED: LED4 on                  */ break; //VT100
    case TY_CSI_PS('x',   0) :      reportTerminalParms  (         2); break; //VT100
    case TY_CSI_PS('x',   1) :      reportTerminalParms  (         3); break; //VT100

    case TY_CSI_PN('@'      ) : currentScreen->insertChars          (p         ); break;
    case TY_CSI_PN('A'      ) : currentScreen->cursorUp             (p         ); break; //VT100
    case TY_CSI_PN('B'      ) : currentScreen->cursorDown           (p         ); break; //VT100
    case TY_CSI_PN('C'      ) : currentScreen->cursorRight          (p         ); break; //VT100
    case TY_CSI_PN('D'      ) : currentScreen->cursorLeft           (p         ); break; //VT100
    case TY_CSI_PN('G'      ) : currentScreen->setCursorX           (p         ); break; //LINUX
    case TY_CSI_PN('H'      ) : currentScreen->setCursorYX          (p,      q); break; //VT100
    case TY_CSI_PN('I'      ) : currentScreen->Tabulate             (p         ); break;
    case TY_CSI_PN('L'      ) : currentScreen->insertLines          (p         ); break;
    case TY_CSI_PN('M'      ) : currentScreen->deleteLines          (p         ); break;
    case TY_CSI_PN('P'      ) : currentScreen->deleteChars          (p         ); break;
    case TY_CSI_PN('S'      ) : currentScreen->scrollUp             (p         ); break;
    case TY_CSI_PN('T'      ) : currentScreen->scrollDown           (p         ); break;
    case TY_CSI_PN('X'      ) : currentScreen->eraseChars           (p         ); break;
    case TY_CSI_PN('Z'      ) : currentScreen->backTabulate         (p         ); break;
    case TY_CSI_PN('c'      ) :      reportTerminalType   (          ); break; //VT100
    case TY_CSI_PN('d'      ) : currentScreen->setCursorY           (p         ); break; //LINUX
    case TY_CSI_PN('f'      ) : currentScreen->setCursorYX          (p,      q); break; //VT100
    case TY_CSI_PN('r'      ) :      setMargins           (p,      q); break; //VT100
    case TY_CSI_PN('y'      ) : /* IGNORED: Confidence test          */ break; //VT100

    case TY_CSI_PR('h',   1) :          setMode      (MODE_AppCuKeys); break; //VT100
    case TY_CSI_PR('l',   1) :        resetMode      (MODE_AppCuKeys); break; //VT100
    case TY_CSI_PR('s',   1) :         saveMode      (MODE_AppCuKeys); break; //FIXME
    case TY_CSI_PR('r',   1) :      restoreMode      (MODE_AppCuKeys); break; //FIXME

    case TY_CSI_PR('l',   2) :        resetMode      (MODE_Ansi     ); break; //VT100

    case TY_CSI_PR('h',   3) : setColumns (132) ; 
			       			   clearEntireScreen();
			       			   setDefaultMargins(); 
			       			   currentScreen->setCursorYX(0,0);
			       					       break; 							  //VT100
    case TY_CSI_PR('l',   3) : setColumns (80)  ; 
			       			   clearEntireScreen(); 	
			       			   setDefaultMargins();
							   currentScreen->setCursorYX(0,0);
			       					       break; 							  //VT100

    case TY_CSI_PR('h',   4) : /* IGNORED: soft scrolling           */ break; //VT100
    case TY_CSI_PR('l',   4) : /* IGNORED: soft scrolling           */ break; //VT100

    case TY_CSI_PR('h',   5) : currentScreen->    setMode      (MODE_Screen   ); break; //VT100
    case TY_CSI_PR('l',   5) : currentScreen->  resetMode      (MODE_Screen   ); break; //VT100

    case TY_CSI_PR('h',   6) : currentScreen->    setMode      (MODE_Origin   ); break; //VT100
    case TY_CSI_PR('l',   6) : currentScreen->  resetMode      (MODE_Origin   ); break; //VT100
    case TY_CSI_PR('s',   6) : currentScreen->   saveMode      (MODE_Origin   ); break; //FIXME
    case TY_CSI_PR('r',   6) : currentScreen->restoreMode      (MODE_Origin   ); break; //FIXME

    case TY_CSI_PR('h',   7) : currentScreen->    setMode      (MODE_Wrap     ); break; //VT100
    case TY_CSI_PR('l',   7) : currentScreen->  resetMode      (MODE_Wrap     ); break; //VT100
    case TY_CSI_PR('s',   7) : currentScreen->   saveMode      (MODE_Wrap     ); break; //FIXME
    case TY_CSI_PR('r',   7) : currentScreen->restoreMode      (MODE_Wrap     ); break; //FIXME

    case TY_CSI_PR('h',   8) : /* IGNORED: autorepeat on            */ break; //VT100
    case TY_CSI_PR('l',   8) : /* IGNORED: autorepeat off           */ break; //VT100
    case TY_CSI_PR('s',   8) : /* IGNORED: autorepeat on            */ break; //VT100
    case TY_CSI_PR('r',   8) : /* IGNORED: autorepeat off           */ break; //VT100

    case TY_CSI_PR('h',   9) : /* IGNORED: interlace                */ break; //VT100
    case TY_CSI_PR('l',   9) : /* IGNORED: interlace                */ break; //VT100
    case TY_CSI_PR('s',   9) : /* IGNORED: interlace                */ break; //VT100
    case TY_CSI_PR('r',   9) : /* IGNORED: interlace                */ break; //VT100

    case TY_CSI_PR('h',  12) : /* IGNORED: Cursor blink             */ break; //att610
    case TY_CSI_PR('l',  12) : /* IGNORED: Cursor blink             */ break; //att610
    case TY_CSI_PR('s',  12) : /* IGNORED: Cursor blink             */ break; //att610
    case TY_CSI_PR('r',  12) : /* IGNORED: Cursor blink             */ break; //att610

    case TY_CSI_PR('h',  25) :          setMode      (MODE_Cursor   ); break; //VT100
    case TY_CSI_PR('l',  25) :        resetMode      (MODE_Cursor   ); break; //VT100
    case TY_CSI_PR('s',  25) :         saveMode      (MODE_Cursor   ); break; //VT100
    case TY_CSI_PR('r',  25) :      restoreMode      (MODE_Cursor   ); break; //VT100

    case TY_CSI_PR('h',  41) : /* IGNORED: obsolete more(1) fix     */ break; //XTERM
    case TY_CSI_PR('l',  41) : /* IGNORED: obsolete more(1) fix     */ break; //XTERM
    case TY_CSI_PR('s',  41) : /* IGNORED: obsolete more(1) fix     */ break; //XTERM
    case TY_CSI_PR('r',  41) : /* IGNORED: obsolete more(1) fix     */ break; //XTERM

    case TY_CSI_PR('h',  47) :          setMode      (MODE_AppScreen); break; //VT100
    case TY_CSI_PR('l',  47) :        resetMode      (MODE_AppScreen); break; //VT100
    case TY_CSI_PR('s',  47) :         saveMode      (MODE_AppScreen); break; //XTERM
    case TY_CSI_PR('r',  47) :      restoreMode      (MODE_AppScreen); break; //XTERM

    case TY_CSI_PR('h',  67) : /* IGNORED: DECBKM                   */ break; //XTERM
    case TY_CSI_PR('l',  67) : /* IGNORED: DECBKM                   */ break; //XTERM
    case TY_CSI_PR('s',  67) : /* IGNORED: DECBKM                   */ break; //XTERM
    case TY_CSI_PR('r',  67) : /* IGNORED: DECBKM                   */ break; //XTERM

    // XTerm defines the following modes:
    // SET_VT200_MOUSE             1000
    // SET_VT200_HIGHLIGHT_MOUSE   1001
    // SET_BTN_EVENT_MOUSE         1002
    // SET_ANY_EVENT_MOUSE         1003
    //
    
    //Note about mouse modes:
    //There are four mouse modes which xterm-compatible terminals can support - 1000,1001,1002,1003
    //Konsole currently supports mode 1000 (basic mouse press and release) and mode 1002 (dragging the mouse).
    //TODO:  Implementation of mouse modes 1001 (something called hilight tracking) and 
    //1003 (a slight variation on dragging the mouse)
    //
 
    case TY_CSI_PR('h', 1000) :          setMode      (MODE_Mouse1000); break; //XTERM
    case TY_CSI_PR('l', 1000) :        resetMode      (MODE_Mouse1000); break; //XTERM
    case TY_CSI_PR('s', 1000) :         saveMode      (MODE_Mouse1000); break; //XTERM
    case TY_CSI_PR('r', 1000) :      restoreMode      (MODE_Mouse1000); break; //XTERM

    case TY_CSI_PR('h', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM
    case TY_CSI_PR('l', 1001) :        resetMode      (MODE_Mouse1001); break; //XTERM
    case TY_CSI_PR('s', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM
    case TY_CSI_PR('r', 1001) : /* IGNORED: hilite mouse tracking    */ break; //XTERM

    case TY_CSI_PR('h', 1002) :          setMode      (MODE_Mouse1002); break; //XTERM
    case TY_CSI_PR('l', 1002) :        resetMode      (MODE_Mouse1002); break; //XTERM
    case TY_CSI_PR('s', 1002) :         saveMode      (MODE_Mouse1002); break; //XTERM
    case TY_CSI_PR('r', 1002) :      restoreMode      (MODE_Mouse1002); break; //XTERM

    case TY_CSI_PR('h', 1003) :          setMode      (MODE_Mouse1003); break; //XTERM
    case TY_CSI_PR('l', 1003) :        resetMode      (MODE_Mouse1003); break; //XTERM
    case TY_CSI_PR('s', 1003) :         saveMode      (MODE_Mouse1003); break; //XTERM
    case TY_CSI_PR('r', 1003) :      restoreMode      (MODE_Mouse1003); break; //XTERM

    case TY_CSI_PR('h', 1047) :          setMode      (MODE_AppScreen); break; //XTERM
    case TY_CSI_PR('l', 1047) : screen[1]->clearEntireScreen(); resetMode(MODE_AppScreen); break; //XTERM
    case TY_CSI_PR('s', 1047) :         saveMode      (MODE_AppScreen); break; //XTERM
    case TY_CSI_PR('r', 1047) :      restoreMode      (MODE_AppScreen); break; //XTERM

    //FIXME: Unitoken: save translations
    case TY_CSI_PR('h', 1048) :      saveCursor           (          ); break; //XTERM
    case TY_CSI_PR('l', 1048) :      restoreCursor        (          ); break; //XTERM
    case TY_CSI_PR('s', 1048) :      saveCursor           (          ); break; //XTERM
    case TY_CSI_PR('r', 1048) :      restoreCursor        (          ); break; //XTERM

    //FIXME: every once new sequences like this pop up in xterm.
    //       Here's a guess of what they could mean.
    case TY_CSI_PR('h', 1049) : saveCursor(); screen[1]->clearEntireScreen(); setMode(MODE_AppScreen); break; //XTERM
    case TY_CSI_PR('l', 1049) : resetMode(MODE_AppScreen); restoreCursor(); break; //XTERM

    //FIXME: weird DEC reset sequence
    case TY_CSI_PE('p'      ) : /* IGNORED: reset         (        ) */ break;

    //FIXME: when changing between vt52 and ansi mode evtl do some resetting.
    case TY_VT52('A'      ) : currentScreen->cursorUp             (         1); break; //VT52
    case TY_VT52('B'      ) : currentScreen->cursorDown           (         1); break; //VT52
    case TY_VT52('C'      ) : currentScreen->cursorRight          (         1); break; //VT52
    case TY_VT52('D'      ) : currentScreen->cursorLeft           (         1); break; //VT52

    case TY_VT52('F'      ) :      setAndUseCharset     (0,    '0'); break; //VT52
    case TY_VT52('G'      ) :      setAndUseCharset     (0,    'B'); break; //VT52

    case TY_VT52('H'      ) : currentScreen->setCursorYX          (1,1       ); break; //VT52
    case TY_VT52('I'      ) : currentScreen->reverseIndex         (          ); break; //VT52
    case TY_VT52('J'      ) : currentScreen->clearToEndOfScreen   (          ); break; //VT52
    case TY_VT52('K'      ) : currentScreen->clearToEndOfLine     (          ); break; //VT52
    case TY_VT52('Y'      ) : currentScreen->setCursorYX          (p-31,q-31 ); break; //VT52
    case TY_VT52('Z'      ) :      reportTerminalType   (           ); break; //VT52
    case TY_VT52('<'      ) :          setMode      (MODE_Ansi     ); break; //VT52
    case TY_VT52('='      ) :          setMode      (MODE_AppKeyPad); break; //VT52
    case TY_VT52('>'      ) :        resetMode      (MODE_AppKeyPad); break; //VT52

    case TY_CSI_PG('c'      ) :  reportSecondaryAttributes(          ); break; //VT100

    default : ReportErrorToken();    break;
  };
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                          Terminal to Host protocol                        */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/* 
   Outgoing bytes originate from several sources:

   - Replies to Enquieries.
   - Mouse Events
   - Keyboard Events
*/

/*!
*/

void TEmuVt102::sendString(const char* s)
{
  emit sendBlock(s,strlen(s));
}

// Replies ----------------------------------------------------------------- --

// This section copes with replies send as response to an enquiery control code.

/*!
*/

void TEmuVt102::reportCursorPosition()
{ char tmp[20];
  sprintf(tmp,"\033[%d;%dR",currentScreen->getCursorY()+1,currentScreen->getCursorX()+1);
  sendString(tmp);
}

/*
   What follows here is rather obsolete and faked stuff.
   The correspondent enquieries are neverthenless issued.
*/

/*!
*/

void TEmuVt102::reportTerminalType()
{
  // Primary device attribute response (Request was: ^[[0c or ^[[c (from TT321 Users Guide))
  //   VT220:  ^[[?63;1;2;3;6;7;8c   (list deps on emul. capabilities)
  //   VT100:  ^[[?1;2c
  //   VT101:  ^[[?1;0c
  //   VT102:  ^[[?6v
  if (getMode(MODE_Ansi))
    sendString("\033[?1;2c");     // I'm a VT100
  else
    sendString("\033/Z");         // I'm a VT52
}

void TEmuVt102::reportSecondaryAttributes()
{
  // Seconday device attribute response (Request was: ^[[>0c or ^[[>c)
  if (getMode(MODE_Ansi))
    sendString("\033[>0;115;0c"); // Why 115?  ;)
  else
    sendString("\033/Z");         // FIXME I don't think VT52 knows about it but kept for
                                  // konsoles backward compatibility.
}

void TEmuVt102::reportTerminalParms(int p)
// DECREPTPARM
{ char tmp[100];
  sprintf(tmp,"\033[%d;1;1;112;112;1;0x",p); // not really true.
  sendString(tmp);
}

/*!
*/

void TEmuVt102::reportStatus()
{
  sendString("\033[0n"); //VT100. Device status report. 0 = Ready.
}

/*!
*/

#define ANSWER_BACK "" // This is really obsolete VT100 stuff.

void TEmuVt102::reportAnswerBack()
{
  sendString(ANSWER_BACK);
}

// Mouse Handling ---------------------------------------------------------- --

/*!
    Mouse clicks are possibly reported to the client
    application if it has issued interest in them.
    They are normally consumed by the widget for copy
    and paste, but may be propagated from the widget
    when gui->setMouseMarks is set via setMode(MODE_Mouse1000).

    `x',`y' are 1-based.
    `ev' (event) indicates the button pressed (0-2)
                 or a general mouse release (3).

    eventType represents the kind of mouse action that occurred:
    	0 = Mouse button press or release
	1 = Mouse drag
*/

void TEmuVt102::onMouse( int cb, int cx, int cy , int eventType )
{ char tmp[20];
  if (  cx<1 || cy<1 ) return;
  // normal buttons are passed as 0x20 + button,
  // mouse wheel (buttons 4,5) as 0x5c + button
  if (cb >= 4) cb += 0x3c;

  //Mouse motion handling
  if ( (getMode(MODE_Mouse1002) || getMode(MODE_Mouse1003)) && eventType == 1 )
	  cb += 0x20; //add 32 to signify motion event

  sprintf(tmp,"\033[M%c%c%c",cb+0x20,cx+0x20,cy+0x20);
  sendString(tmp);
}

// Keyboard Handling ------------------------------------------------------- --

#if defined(HAVE_XKB)
static void scrolllock_set_off();
static void scrolllock_set_on();
#endif

void TEmuVt102::scrollLock(const bool lock)
{
  if (lock)
  {
    holdScreen = true;
    emit lockPty(true);
  }
  else
  {
    holdScreen = false;
    emit lockPty(false);
  }
#if defined(HAVE_XKB)
  if (holdScreen)
    scrolllock_set_on();
  else
    scrolllock_set_off();
#endif
}

void TEmuVt102::onScrollLock()
{
  bool switchlock = !holdScreen;
  scrollLock(switchlock);
}

#define encodeMode(M,B) BITS(B,getMode(M))
#define encodeStat(M,B) BITS(B,((ev->modifiers() & (M)) == (M)))

/*
   Keyboard event handling has been simplified somewhat by pushing
   the complications towards a configuration file [see KeyTrans class].
*/

void TEmuVt102::onKeyPress( QKeyEvent* ev )
{
  if (!listenToKeyPress) return; // someone else gets the keys

//printf("State/Key: 0x%04x 0x%04x (%d,%d)\n",ev->state(),ev->key(),ev->text().length(),ev->text().length()?ev->text().ascii()[0]:0);

  // lookup in keyboard translation table ...
  int cmd = CMD_none; 
  QByteArray txt; 
  bool metaspecified;
  if (keytrans->findEntry(ev->key(), encodeMode(MODE_NewLine  , BITS_NewLine   ) + // OLD,
                                     encodeMode(MODE_Ansi     , BITS_Ansi      ) + // OBSOLETE,
                                     encodeMode(MODE_AppCuKeys, BITS_AppCuKeys ) + // VT100 stuff
                                     encodeMode(MODE_AppScreen, BITS_AppScreen ) + // VT100 stuff
                                     encodeStat(Qt::ControlModifier , BITS_Control   ) +
                                     encodeStat(Qt::ShiftModifier   , BITS_Shift     ) +
                                     encodeStat(Qt::AltModifier     , BITS_Alt       ),
                          &cmd, txt, &metaspecified ))
//printf("cmd: %d, %s, %d\n",cmd,txt,len);
  
  switch(cmd) // ... and execute if found.
  {
    #warning "Add functionality elsewhere to handle scrolling of views when up/down/page-up/page-down keys are pressed."
    //case CMD_scrollPageUp   : scrollViewPages(-1); return;
    //case CMD_scrollPageDown : scrollViewPages(+1); return;
    //case CMD_scrollLineUp   : scrollView(-1             ); return;
    //case CMD_scrollLineDown : scrollView(+1             ); return;
    case CMD_scrollLock     : onScrollLock(                ); return;
  }
  if (holdScreen)
  {
    switch(ev->key())
    {
    //case Qt::Key_Down : scrollView(+1); return;
    //case Qt::Key_Up : scrollView(-1); return;
    //case Qt::Key_PageUp : scrollViewPages(-1); return;
    //case Qt::Key_PageDown : scrollViewPages(+1); return;
    }
  }
  
  // revert to non-history when typing
  if (currentScreen->getHistCursor() != currentScreen->getHistLines() && (!ev->text().isEmpty()
    || ev->key()==Qt::Key_Down || ev->key()==Qt::Key_Up || ev->key()==Qt::Key_Left || ev->key()==Qt::Key_Right
    || ev->key()==Qt::Key_PageUp || ev->key()==Qt::Key_PageDown))
    currentScreen->setHistCursor(currentScreen->getHistLines());

  if (cmd==CMD_send) {
    if ((ev->modifiers() & Qt::AltModifier) && !metaspecified ) sendString("\033");
    emit sendBlock(txt.constData(), txt.length());
    return;
  }

  // fall back handling
  if (!ev->text().isEmpty())
  {
    if (ev->modifiers() & Qt::AltModifier) sendString("\033"); // ESC, this is the ALT prefix
    QByteArray s = m_codec->fromUnicode(ev->text());     // encode for application
    // FIXME: In Qt 2, QKeyEvent::text() would return "\003" for Ctrl-C etc.
    //        while in Qt 3 it returns the actual key ("c" or "C") which caused
    //        the ControlButton to be ignored. This hack seems to work for
    //        latin1 locales at least. Please anyone find a clean solution (malte)
    if (ev->modifiers() & Qt::ControlModifier)
      s.fill(ev->text().toAscii()[0], 1);
    emit sendBlock(s.data(),s.length());              // we may well have s.length() > 1 
    return;
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

   This conversion it eventually continued in TEWidget.C, since 
   it might involve VT100 enhanced fonts, which have these
   particular glyphs allocated in (0x00-0x1f) in their code page.
*/

#define CHARSET charset[currentScreen==screen[1]]

// Apply current character map.

unsigned short TEmuVt102::applyCharset(unsigned short c)
{
  if (CHARSET.graphic && 0x5f <= c && c <= 0x7e) return vt100_graphics[c-0x5f];
  if (CHARSET.pound                && c == '#' ) return 0xa3; //This mode is obsolete
  return c;
}

/*
   "Charset" related part of the emulation state.
   This configures the VT100 charset filter.

   While most operation work on the current screen,
   the following two are different.
*/

void TEmuVt102::resetCharset(int scrno)
{
  charset[scrno].cu_cs   = 0;
  strncpy(charset[scrno].charset,"BBBB",4);
  charset[scrno].sa_graphic = false;
  charset[scrno].sa_pound   = false;
  charset[scrno].graphic = false;
  charset[scrno].pound   = false;
}

/*!
*/

void TEmuVt102::setCharset(int n, int cs) // on both screens.
{
  charset[0].charset[n&3] = cs; useCharset(charset[0].cu_cs);
  charset[1].charset[n&3] = cs; useCharset(charset[1].cu_cs);
}

/*!
*/

void TEmuVt102::setAndUseCharset(int n, int cs)
{
  CHARSET.charset[n&3] = cs;
  useCharset(n&3);
}

/*!
*/

void TEmuVt102::useCharset(int n)
{
  CHARSET.cu_cs   = n&3;
  CHARSET.graphic = (CHARSET.charset[n&3] == '0');
  CHARSET.pound   = (CHARSET.charset[n&3] == 'A'); //This mode is obsolete
}

void TEmuVt102::setDefaultMargins()
{
	screen[0]->setDefaultMargins();
	screen[1]->setDefaultMargins();
}

void TEmuVt102::setMargins(int t, int b)
{
  screen[0]->setMargins(t, b);
  screen[1]->setMargins(t, b);
}

/*! Save the cursor position and the rendition attribute settings. */

void TEmuVt102::saveCursor()
{
  CHARSET.sa_graphic = CHARSET.graphic;
  CHARSET.sa_pound   = CHARSET.pound; //This mode is obsolete
  // we are not clear about these
  //sa_charset = charsets[cScreen->charset];
  //sa_charset_num = cScreen->charset;
  currentScreen->saveCursor();
}

/*! Restore the cursor position and the rendition attribute settings. */

void TEmuVt102::restoreCursor()
{
  CHARSET.graphic = CHARSET.sa_graphic;
  CHARSET.pound   = CHARSET.sa_pound; //This mode is obsolete
  currentScreen->restoreCursor();
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                                Mode Operations                            */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*
   Some of the emulations state is either added to the state of the screens.

   This causes some scoping problems, since different emulations choose to
   located the mode either to the current screen or to both.

   For strange reasons, the extend of the rendition attributes ranges over
   all screens and not over the actual screen.

   We decided on the precise precise extend, somehow.
*/

// "Mode" related part of the state. These are all booleans.

void TEmuVt102::resetModes()
{
  resetMode(MODE_Mouse1000); saveMode(MODE_Mouse1000);
  resetMode(MODE_Mouse1001); saveMode(MODE_Mouse1001);
  resetMode(MODE_Mouse1002); saveMode(MODE_Mouse1002);
  resetMode(MODE_Mouse1003); saveMode(MODE_Mouse1003);

  resetMode(MODE_AppScreen); saveMode(MODE_AppScreen);
  // here come obsolete modes
  resetMode(MODE_AppCuKeys); saveMode(MODE_AppCuKeys);
  resetMode(MODE_NewLine  );
    setMode(MODE_Ansi     );
  holdScreen = false;
}

void TEmuVt102::setMode(int m)
{
  currParm.mode[m] = true;
  switch (m)
  {
    case MODE_Mouse1000:
    case MODE_Mouse1001:
    case MODE_Mouse1002:
    case MODE_Mouse1003:
 	    emit programUsesMouse(false); 
    break;

    case MODE_AppScreen : screen[1]->clearSelection();
                          setScreen(1);
    break;
  }
  if (m < MODES_SCREEN || m == MODE_NewLine)
  {
    screen[0]->setMode(m);
    screen[1]->setMode(m);
  }
}

void TEmuVt102::resetMode(int m)
{
  currParm.mode[m] = false;
  switch (m)
  {
    case MODE_Mouse1000 : 
    case MODE_Mouse1001 :
    case MODE_Mouse1002 :
    case MODE_Mouse1003 :
	    emit programUsesMouse(true); 
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

void TEmuVt102::saveMode(int m)
{
  saveParm.mode[m] = currParm.mode[m];
}

void TEmuVt102::restoreMode(int m)
{
  if(saveParm.mode[m]) setMode(m); else resetMode(m);
}

bool TEmuVt102::getMode(int m)
{
  return currParm.mode[m];
}

#warning "Code to handle signal/slot connections has already been moved elsewhere, but mouse mode refreshing part below has not yet been looked at.  Neither has the part inside the HAVE_XKB define which calls scrolllock_set_xyz."
/*void TEmuVt102::setConnect(bool c)
{
  TEmulation::setConnect(c);

  QListIterator< TEWidget* > viewIter(_views);
  
  while (viewIter.hasNext())
  {
    QObject::disconnect(viewIter.next(), SIGNAL(sendStringToEmu(const char*)),
                        this, SLOT(sendString(const char*)));
  }
  if (c)
  { // refresh mouse mode
    int mouseModes[4] = {MODE_Mouse1000,MODE_Mouse1001,MODE_Mouse1002,MODE_Mouse1003};
    for (int mode = 0 ; mode < 4 ; mode++)
    {
    	if (getMode(mouseModes[mode]))
      		setMode(mouseModes[mode]);
    	else
      		resetMode(mouseModes[mode]);
    }
#if defined(HAVE_XKB)
    if (holdScreen)
      scrolllock_set_on();
    else
      scrolllock_set_off();
#endif

    QListIterator< TEWidget* > viewIter2(_views);

    while (viewIter2.hasNext())
    {
        QObject::connect(viewIter2.next(), SIGNAL(sendStringToEmu(const char*)),
                         this, SLOT(sendString(const char*)));
    }
  }
}*/

char TEmuVt102::getErase()
{
  int cmd = CMD_none; 
  QByteArray txt; 
  bool metaspecified;
  
  if (keytrans->findEntry(Qt::Key_Backspace, 0, &cmd, txt,
      &metaspecified) && (cmd==CMD_send) && (txt.length() == 1))
    return txt[0];
    
  return '\b';
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/*                               Diagnostic                                  */
/*                                                                           */
/* ------------------------------------------------------------------------- */

/*! shows the contents of the scan buffer.

    This functions is used for diagnostics. It is called by \e ReportErrorToken
    to inform about strings that cannot be decoded or handled by the emulation.

    \sa ReportErrorToken
*/

/*!
*/

static void hexdump(int* s, int len)
{ int i;
  for (i = 0; i < len; i++)
  {
    if (s[i] == '\\')
      printf("\\\\");
    else
    if ((s[i]) > 32 && s[i] < 127)
      printf("%c",s[i]);
    else
      printf("\\%04x(hex)",s[i]);
  }
}

void TEmuVt102::scan_buffer_report()
{
  if (ppos == 0 || ppos == 1 && (pbuf[0] & 0xff) >= 32) return;
  printf("token: "); hexdump(pbuf,ppos); printf("\n");
}

/*!
*/

void TEmuVt102::ReportErrorToken()
{
#ifndef NDEBUG
  printf("undecodable "); scan_buffer_report();
#endif
}

/*
 Originally comes from NumLockX http://dforce.sh.charactervut.characterz/~seli/en/numlockx

 NumLockX
 
 Copyright (C) 2000-2001 Lubos Lunak        <l.lunak@kde.org>
 Copyright (C) 2001      Oswald Buddenhagen <ossi@kde.org>

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the "Software"),
to deal in the Software without restriction, including without limitation
the rights to use, copy, modify, merge, publish, distribute, sublicense,
and/or sell copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

****************************************************************************/

#if defined(HAVE_XKB)

#include <X11/Xlib.h>

#define explicit myexplicit
#include <X11/XKBlib.h>
#undef explicit

#include <X11/keysym.h>
#include <QX11Info>

/* the XKB stuff is based on code created by Oswald Buddenhagen <ossi@kde.org> */
static int xkb_init()
{
    int xkb_opcode, xkb_event, xkb_error;
    int xkb_lmaj = XkbMajorVersion;
    int xkb_lmin = XkbMinorVersion;
    return XkbLibraryVersion( &xkb_lmaj, &xkb_lmin )
        && XkbQueryExtension( QX11Info::display(), &xkb_opcode, &xkb_event, &xkb_error,
			       &xkb_lmaj, &xkb_lmin );
}
    
#if 0
// This method doesn't work in all cases. The atom "ScrollLock" doesn't seem
// to exist on all XFree versions (at least it's not here with my 3.3.6) - DF
static unsigned int xkb_mask_modifier( XkbDescPtr xkb, const char *name )
{
    int i;
    if( !xkb || !xkb->names )
	return 0;

    Atom atom = XInternAtom( xkb->dpy, name, true );
    if (atom == None)
        return 0;

    for( i = 0;
         i < XkbNumVirtualMods;
	 i++ )
    {
	if (atom == xkb->names->vmods[i] )
	{
	    unsigned int mask;
	    XkbVirtualModsToReal( xkb, 1 << i, &mask );
	    return mask;
	}
    }
    return 0;
}

static unsigned int xkb_scrolllock_mask()
{
    XkbDescPtr xkb;
    if(( xkb = XkbGetKeyboard( QX11Info::display(), XkbAllComponentsMask, XkbUseCoreKbd )) != NULL )
    {
        unsigned int mask = xkb_mask_modifier( xkb, "ScrollLock" );
        XkbFreeKeyboard( xkb, 0, True );
        return mask;
    }
    return 0;
}

#else
static unsigned int xkb_scrolllock_mask()
{
    int scrolllock_mask = 0;
    XModifierKeymap* map = XGetModifierMapping( QX11Info::display() );
    KeyCode scrolllock_keycode = XKeysymToKeycode( QX11Info::display(), XK_Scroll_Lock );
    if( scrolllock_keycode == NoSymbol ) {
        XFreeModifiermap(map);
        return 0;
    }
    for( int i = 0;
         i < 8;
         ++i )
        {
       if( map->modifiermap[ map->max_keypermod * i ] == scrolllock_keycode )
               scrolllock_mask += 1 << i;
       }

    XFreeModifiermap(map);
    return scrolllock_mask;
}
#endif


static unsigned int scrolllock_mask = 0;
        
static int xkb_set_on()
{
    if (!scrolllock_mask)
    {
       if( !xkb_init())
          return 0;
       scrolllock_mask = xkb_scrolllock_mask();
       if( scrolllock_mask == 0 )
          return 0;
    }
    XkbLockModifiers ( QX11Info::display(), XkbUseCoreKbd, scrolllock_mask, scrolllock_mask);
    return 1;
}
    
static int xkb_set_off()
{
    if (!scrolllock_mask)
    {
       if( !xkb_init())
          return 0;
       scrolllock_mask = xkb_scrolllock_mask();
       if( scrolllock_mask == 0 )
          return 0;
    }
    XkbLockModifiers ( QX11Info::display(), XkbUseCoreKbd, scrolllock_mask, 0);
    return 1;
}

static void scrolllock_set_on()
{
    xkb_set_on();
}

static void scrolllock_set_off()
{
    xkb_set_off();
}
#endif // defined(HAVE_XKB)

#include "TEmuVt102.moc"
