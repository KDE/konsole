/* -------------------------------------------------------------------------- */
/*                                                                            */
/* [keytrans.C]             Keyboard Translation                              */
/*                                                                            */
/* -------------------------------------------------------------------------- */
/*                                                                            */
/* Copyright (c) 1997,1998 by Lars Doelle <lars.doelle@on-line.de>            */
/*                                                                            */
/* This file is part of Konsole - an X terminal for KDE                       */
/*                                                                            */
/* -------------------------------------------------------------------------- */

/* Still TODO:
   - handle errors more gracefully.
   - put into configuration
*/

/*
   The keyboard translation table allows to configure konsoles behavior
   on key strokes.
*/

#include "keytrans.h"
#include <qnamespace.h>
#include <qbuffer.h>
#include <qobject.h>
#include <qdict.h>
#include <qintdict.h>
#include <qfile.h>
#include <kglobal.h>
#include <kstddirs.h>
#include <klocale.h>

#include <stdio.h>

#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)

// KeyEntry -

KeyTrans::KeyEntry::KeyEntry(int _key, int _bits, int _mask, int _cmd, QString _txt)
: key(_key), bits(_bits), mask(_mask), cmd(_cmd), txt(_txt)
{
}

KeyTrans::KeyEntry::~KeyEntry()
{
}

bool KeyTrans::KeyEntry::matches(int _key, int _bits, int _mask)
{ int m = mask & _mask;
  return _key == key && (bits & m) == (_bits & m);
}

QString KeyTrans::KeyEntry::text()
{
  return txt;
}

// KeyTrans -

KeyTrans::KeyTrans()
{
  path = "";
  numb = 0;
}

KeyTrans::~KeyTrans()
{
}

void KeyTrans::addEntry(int key, int bits, int mask, int cmd, QString txt)
{
  for (QListIterator<KeyEntry> it(table); it.current(); ++it)
  {
    if (it.current()->matches(key,bits,mask))
    {
      fprintf(stderr,"rejecting key %d bits 0x%02x mask 0x%02x:"
                     " conflict with earlier entry.\n",key,bits,mask);
      return;
    }
  }
  table.append(new KeyEntry(key,bits,mask,cmd,txt));
}

bool KeyTrans::findEntry(int key, int bits, int* cmd, const char** txt, int* len)
{
  for (QListIterator<KeyEntry> it(table); it.current(); ++it)
    if (it.current()->matches(key,bits,0xffff))
    {
      *cmd = it.current()->cmd;
      *txt = it.current()->txt.ascii();
      *len = it.current()->txt.length();
      return TRUE;
    }
  return FALSE;
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Scanner for keyboard configuration                                        */
/*                                                                           */
/* ------------------------------------------------------------------------- */

// Ok, we have a regular tokenizer here.
/* Tokens
   - Spaces
   - Name
   - String
   - Opr     +-:
*/

#define SYMName    0
#define SYMString  1
#define SYMEol     2
#define SYMEof     3
#define SYMOpr     4

#define inRange(L,X,H) ((L <= X) && (X <= H))
#define isNibble(X) (inRange('A',X,'F')||inRange('a',X,'f')||inRange('0',X,'9'))
#define convNibble(X) (inRange('0',X,'9')?X-'0':X+10-(inRange('A',X,'F')?'A':'a'))

//FIXME: we might give these variables a prefix.
static int     startofsym;
static int     sym;
static QString res;
static int     len;
static int     cc;
static int     lineno;


static bool getSymbol(QIODevice &buf)
{
  res = ""; len = 0;
  while (cc == ' ')
    cc = buf.getch(); // skip spaces
  if (cc == '#')      // skip comment
  {
    while (cc != '\n' && cc > 0)
      cc = buf.getch();
  }
  startofsym = lineno;
  if (cc <= 0)
  {
    sym = SYMEof;
    return FALSE; // eos
  }
  if (cc == '\n')
  {
    lineno += 1;
    sym = SYMEol;
    cc = buf.getch();
    return TRUE; // eol
  }
  if (inRange('A',cc,'Z')||inRange('a',cc,'z')||inRange('0',cc,'9'))
  {
    sym = SYMName;
    while (inRange('A',cc,'Z') || inRange('a',cc,'z') || inRange('0',cc,'9'))
    {
      res = res + (char)cc;
      cc = buf.getch();
    }
    return TRUE;
  }
  if (strchr("+-:",cc))
  {
    sym = SYMOpr;
    res = (char)cc;
    cc = buf.getch();
    return TRUE;
  }
  if (cc == '"')
  {
    cc = buf.getch();
    while (cc >= ' ' && cc != '"')
    { int sc;
      if (cc == '\\') // handle quotation
      {
        cc = buf.getch();
        switch (cc)
        {
          case 'E'  : sc = 27; cc = buf.getch(); break;
          case 'b'  : sc =  8; cc = buf.getch(); break;
          case 'f'  : sc = 12; cc = buf.getch(); break;
          case 't'  : sc =  9; cc = buf.getch(); break;
          case 'r'  : sc = 13; cc = buf.getch(); break;
          case 'n'  : sc = 10; cc = buf.getch(); break;
          case '\\' : // fall thru
          case '"'  : sc = cc; cc = buf.getch(); break;
          case 'x'  : cc = buf.getch();
                      sc = 0;
                      if (isNibble(cc)) { sc = 16*sc + convNibble(cc); cc = buf.getch(); } else goto ERROR;
                      if (isNibble(cc)) { sc = 16*sc + convNibble(cc); cc = buf.getch(); } else goto ERROR;
                      break;
          default   : goto ERROR;
        }
      }
      else
      {
        // regular char
        sc = cc; cc = buf.getch();
      }
      res = res + (char)sc;
      len = len + 1;
    }
    if (cc != '"') goto ERROR;
    cc = buf.getch();
    sym = SYMString;
    return TRUE;
  }
  ERROR:
  /* Error processing is so, that we skip all errorness lines.
     This is stable, since no other errors can follow from this behavior.
    //FIXME: not done right, yet.
  */
    fprintf(stderr,"error reading keytab line %d.\n",startofsym);
    fprintf(stderr,"text following: ");
    while (cc != '\n' && cc > 0)
    {
      fprintf(stderr,"%c",cc);
      cc = buf.getch();
    }
    fprintf(stderr,"\n");
    return FALSE;
}

static void ReportToken() // diagnostic
{
  printf("sym(%d): ",startofsym);
  switch(sym)
  {
    case SYMEol    : printf("End of line"); break;
    case SYMEof    : printf("End of file"); break;
    case SYMName   : printf("Name: %s",res.ascii()); break;
    case SYMOpr    : printf("Opr : %s",res.ascii()); break;
    case SYMString : printf("String len %d,%d ",res.length(),len);
                     for (unsigned i = 0; i < res.length(); i++)
                       printf(" %02x(%c)",res.ascii()[i],res.ascii()[i]>=' '?res.ascii()[i]:'?');
                     break;
  }
  printf("\n");
}

// local symbol tables ---------------------------------------------------------------------

class KeyTransSymbols
{
public:
  KeyTransSymbols();
public:
  QDict<QObject> keysyms;
  QDict<QObject> modsyms;
  QDict<QObject> oprsyms;
};

static KeyTransSymbols syms;

// parser ----------------------------------------------------------------------------------
/* Syntax
   - Line :: [KeyName { ("+" | "-") ModeName } ":" (String|CommandName)] "\n"
   - Comment :: '#' (any but \n)*
*/

KeyTrans* KeyTrans::fromDevice(QString path, QIODevice &buf)
{
  KeyTrans* kt = new KeyTrans;
  kt->path = path;

  // Opening sequence

  buf.open(IO_ReadOnly);
  cc = buf.getch();
  lineno = 1;
  getSymbol(buf);

Loop:
  // syntax: ["key" KeyName { ("+" | "-") ModeName } ":" String/CommandName] ["#" Comment]
  if (sym == SYMName && !strcmp(res.ascii(),"keyboard"))
  {
    getSymbol(buf);
    if (sym != SYMString) goto ERROR; // header expected
    kt->hdr = i18n(res);
    getSymbol(buf);
    if (sym != SYMEol)    goto ERROR; // unexpected text
    getSymbol(buf);                   // eoln
    goto Loop;
  }
  if (sym == SYMName && !strcmp(res.ascii(),"key"))
  {
//printf("line %3d: ",startofsym);
    getSymbol(buf);
    // keyname
    if (sym != SYMName) goto ERROR; // mode name expected
    if (!syms.keysyms[res]) goto ERROR; // unknown key
    int key = (int)syms.keysyms[res]-1;
//printf(" key %s (%04x)",res.ascii(),(int)keysyms[res]-1);
    getSymbol(buf); // + - :
    int mode = 0;
    int mask = 0;
    while (sym == SYMOpr && (!strcmp(res.ascii(),"+") || !strcmp(res.ascii(),"-")))
    {
      bool on = !strcmp(res.ascii(),"+");
      getSymbol(buf);
      // mode name
      if (sym != SYMName) goto ERROR; // mode name expected
      if (!syms.modsyms[res]) goto ERROR; // unknown mod
      int bits = (int)syms.modsyms[res]-1;
      mode |= (on << bits);
      mask |= (1 << bits);
//printf(", mode %s(%d) %s",res.ascii(),(int)modsyms[res]-1,on?"on":"off");
      getSymbol(buf);
    }
    if (sym != SYMOpr || strcmp(res.ascii(),":")) goto ERROR; // ":" expected
    getSymbol(buf);
    // string or command
    int cmd = 0;
    if (sym == SYMName)
    {
      if (!syms.oprsyms[res]) goto ERROR; // unknown opr
      cmd = (int)syms.oprsyms[res]-1;
//printf(": do %s(%d)",res.ascii(),(int)oprsyms[res]-1);
    }
    else
    if (sym == SYMString)
    {
      cmd = CMD_send;
//printf(": send");
//for (unsigned i = 0; i < res.length(); i++)
//printf(" %02x(%c)",res.ascii()[i],res.ascii()[i]>=' '?res.ascii()[i]:'?');
    }
    else
      goto ERROR; // command or string expected
//printf(". summary %04x,%02x,%02x,%d\n",key,mode,mask,cmd);
    kt->addEntry(key,mode,mask,cmd,res);
    getSymbol(buf);
    if (sym != SYMEol) goto ERROR; // unexpected text
    getSymbol(buf); // eoln
    goto Loop;
  }
  else
  if (sym == SYMEol)
  {
    getSymbol(buf);
    goto Loop;
  }

  if (sym != SYMEof)
  {
ERROR: printf("ERROR:"); printf("symbol: "); ReportToken();
  }

  buf.close();

  return kt;
}


KeyTrans* KeyTrans::defaultKeyTrans()
{
  QCString txt =
#include "default.keytab.h"
  ;
  QBuffer buf(txt);
  return fromDevice("[buildin]",buf);
}

KeyTrans* KeyTrans::fromFile(const char* path)
{
  QFile file(path);
  return fromDevice(path,file);
}

// local symbol tables ---------------------------------------------------------------------
// material needed for parsing the config file.
// This is incomplete work.

void defKeySym(const char* key, int val)
{
  syms.keysyms.insert(key,(QObject*)(val+1));
}

void defOprSym(const char* key, int val)
{
  syms.oprsyms.insert(key,(QObject*)(val+1));
}

void defModSym(const char* key, int val)
{
  syms.modsyms.insert(key,(QObject*)(val+1));
}

static void defOprSyms()
{
  // Modifier
  defOprSym("scrollLineUp",  CMD_scrollLineUp  );
  defOprSym("scrollLineDown",CMD_scrollLineDown);
  defOprSym("scrollPageUp",  CMD_scrollPageUp  );
  defOprSym("scrollPageDown",CMD_scrollPageDown);
  defOprSym("emitSelection", CMD_emitSelection );
}

static void defModSyms()
{
  // Modifier
  defModSym("Shift",      BITS_Shift        );
  defModSym("Control",    BITS_Control      );
  defModSym("Alt",        BITS_Alt          );
  // Modes
  defModSym("BsHack",     BITS_BsHack       ); // deprecated
  defModSym("Ansi",       BITS_Ansi         );
  defModSym("NewLine",    BITS_NewLine      );
  defModSym("AppCuKeys",  BITS_AppCuKeys    );
}

static void defKeySyms()
{
  // Grey keys
  defKeySym("Escape",       Qt::Key_Escape      );
  defKeySym("Tab",          Qt::Key_Tab         );
  defKeySym("Backtab",      Qt::Key_Backtab     );
  defKeySym("Backspace",    Qt::Key_Backspace   );
  defKeySym("Return",       Qt::Key_Return      );
  defKeySym("Enter",        Qt::Key_Enter       );
  defKeySym("Insert",       Qt::Key_Insert      );
  defKeySym("Delete",       Qt::Key_Delete      );
  defKeySym("Pause",        Qt::Key_Pause       );
  defKeySym("Print",        Qt::Key_Print       );
  defKeySym("SysReq",       Qt::Key_SysReq      );
  defKeySym("Home",         Qt::Key_Home        );
  defKeySym("End",          Qt::Key_End         );
  defKeySym("Left",         Qt::Key_Left        );
  defKeySym("Up",           Qt::Key_Up          );
  defKeySym("Right",        Qt::Key_Right       );
  defKeySym("Down",         Qt::Key_Down        );
  defKeySym("Prior",        Qt::Key_Prior       );
  defKeySym("Next",         Qt::Key_Next        );
  defKeySym("Shift",        Qt::Key_Shift       );
  defKeySym("Control",      Qt::Key_Control     );
  defKeySym("Meta",         Qt::Key_Meta        );
  defKeySym("Alt",          Qt::Key_Alt         );
  defKeySym("CapsLock",     Qt::Key_CapsLock    );
  defKeySym("NumLock",      Qt::Key_NumLock     );
  defKeySym("ScrollLock",   Qt::Key_ScrollLock  );
  defKeySym("F1",           Qt::Key_F1          );
  defKeySym("F2",           Qt::Key_F2          );
  defKeySym("F3",           Qt::Key_F3          );
  defKeySym("F4",           Qt::Key_F4          );
  defKeySym("F5",           Qt::Key_F5          );
  defKeySym("F6",           Qt::Key_F6          );
  defKeySym("F7",           Qt::Key_F7          );
  defKeySym("F8",           Qt::Key_F8          );
  defKeySym("F9",           Qt::Key_F9          );
  defKeySym("F10",          Qt::Key_F10         );
  defKeySym("F11",          Qt::Key_F11         );
  defKeySym("F12",          Qt::Key_F12         );
  defKeySym("F13",          Qt::Key_F13         );
  defKeySym("F14",          Qt::Key_F14         );
  defKeySym("F15",          Qt::Key_F15         );
  defKeySym("F16",          Qt::Key_F16         );
  defKeySym("F17",          Qt::Key_F17         );
  defKeySym("F18",          Qt::Key_F18         );
  defKeySym("F19",          Qt::Key_F19         );
  defKeySym("F20",          Qt::Key_F20         );
  defKeySym("F21",          Qt::Key_F21         );
  defKeySym("F22",          Qt::Key_F22         );
  defKeySym("F23",          Qt::Key_F23         );
  defKeySym("F24",          Qt::Key_F24         );
  defKeySym("F25",          Qt::Key_F25         );
  defKeySym("F26",          Qt::Key_F26         );
  defKeySym("F27",          Qt::Key_F27         );
  defKeySym("F28",          Qt::Key_F28         );
  defKeySym("F29",          Qt::Key_F29         );
  defKeySym("F30",          Qt::Key_F30         );
  defKeySym("F31",          Qt::Key_F31         );
  defKeySym("F32",          Qt::Key_F32         );
  defKeySym("F33",          Qt::Key_F33         );
  defKeySym("F34",          Qt::Key_F34         );
  defKeySym("F35",          Qt::Key_F35         );
  defKeySym("Super_L",      Qt::Key_Super_L     );
  defKeySym("Super_R",      Qt::Key_Super_R     );
  defKeySym("Menu",         Qt::Key_Menu        );
  defKeySym("Hyper_L",      Qt::Key_Hyper_L     );
  defKeySym("Hyper_R",      Qt::Key_Hyper_R     );

  // Regular keys
  defKeySym("Space",        Qt::Key_Space       );
  defKeySym("Exclam",       Qt::Key_Exclam      );
  defKeySym("QuoteDbl",     Qt::Key_QuoteDbl    );
  defKeySym("NumberSign",   Qt::Key_NumberSign  );
  defKeySym("Dollar",       Qt::Key_Dollar      );
  defKeySym("Percent",      Qt::Key_Percent     );
  defKeySym("Ampersand",    Qt::Key_Ampersand   );
  defKeySym("Apostrophe",   Qt::Key_Apostrophe  );
  defKeySym("ParenLeft",    Qt::Key_ParenLeft   );
  defKeySym("ParenRight",   Qt::Key_ParenRight  );
  defKeySym("Asterisk",     Qt::Key_Asterisk    );
  defKeySym("Plus",         Qt::Key_Plus        );
  defKeySym("Comma",        Qt::Key_Comma       );
  defKeySym("Minus",        Qt::Key_Minus       );
  defKeySym("Period",       Qt::Key_Period      );
  defKeySym("Slash",        Qt::Key_Slash       );
  defKeySym("0",            Qt::Key_0           );
  defKeySym("1",            Qt::Key_1           );
  defKeySym("2",            Qt::Key_2           );
  defKeySym("3",            Qt::Key_3           );
  defKeySym("4",            Qt::Key_4           );
  defKeySym("5",            Qt::Key_5           );
  defKeySym("6",            Qt::Key_6           );
  defKeySym("7",            Qt::Key_7           );
  defKeySym("8",            Qt::Key_8           );
  defKeySym("9",            Qt::Key_9           );
  defKeySym("Colon",        Qt::Key_Colon       );
  defKeySym("Semicolon",    Qt::Key_Semicolon   );
  defKeySym("Less",         Qt::Key_Less        );
  defKeySym("Equal",        Qt::Key_Equal       );
  defKeySym("Greater",      Qt::Key_Greater     );
  defKeySym("Question",     Qt::Key_Question    );
  defKeySym("At",           Qt::Key_At          );
  defKeySym("A",            Qt::Key_A           );
  defKeySym("B",            Qt::Key_B           );
  defKeySym("C",            Qt::Key_C           );
  defKeySym("D",            Qt::Key_D           );
  defKeySym("E",            Qt::Key_E           );
  defKeySym("F",            Qt::Key_F           );
  defKeySym("G",            Qt::Key_G           );
  defKeySym("H",            Qt::Key_H           );
  defKeySym("I",            Qt::Key_I           );
  defKeySym("J",            Qt::Key_J           );
  defKeySym("K",            Qt::Key_K           );
  defKeySym("L",            Qt::Key_L           );
  defKeySym("M",            Qt::Key_M           );
  defKeySym("N",            Qt::Key_N           );
  defKeySym("O",            Qt::Key_O           );
  defKeySym("P",            Qt::Key_P           );
  defKeySym("Q",            Qt::Key_Q           );
  defKeySym("R",            Qt::Key_R           );
  defKeySym("S",            Qt::Key_S           );
  defKeySym("T",            Qt::Key_T           );
  defKeySym("U",            Qt::Key_U           );
  defKeySym("V",            Qt::Key_V           );
  defKeySym("W",            Qt::Key_W           );
  defKeySym("X",            Qt::Key_X           );
  defKeySym("Y",            Qt::Key_Y           );
  defKeySym("Z",            Qt::Key_Z           );
  defKeySym("BracketLeft",  Qt::Key_BracketLeft );
  defKeySym("Backslash",    Qt::Key_Backslash   );
  defKeySym("BracketRight", Qt::Key_BracketRight);
  defKeySym("AsciiCircum",  Qt::Key_AsciiCircum );
  defKeySym("Underscore",   Qt::Key_Underscore  );
  defKeySym("QuoteLeft",    Qt::Key_QuoteLeft   );
  defKeySym("BraceLeft",    Qt::Key_BraceLeft   );
  defKeySym("Bar",          Qt::Key_Bar         );
  defKeySym("BraceRight",   Qt::Key_BraceRight  );
  defKeySym("AsciiTilde",   Qt::Key_AsciiTilde  );
}

KeyTransSymbols::KeyTransSymbols()
{
  defModSyms();
  defOprSyms();
  defKeySyms();
}

// Global material -----------------------------------------------------------

static int keytab_serial = 0; //FIXME: remove,localize

static QIntDict<KeyTrans> numb2keymap;
static QDict<KeyTrans>    path2keymap;

KeyTrans* KeyTrans::find(int numb)
{
  KeyTrans* res = numb2keymap.find(numb);
  return res ? res : numb2keymap.find(0);
}

KeyTrans* KeyTrans::find(const char* path)
{
  KeyTrans* res = path2keymap.find(path);
  return res ? res : numb2keymap.find(0);
}

int KeyTrans::count()
{
  return numb2keymap.count();
}

void KeyTrans::addKeyTrans()
{
  this->numb = keytab_serial ++;
  numb2keymap.insert(numb,this);
  path2keymap.insert(path.data(),this);
}

void KeyTrans::loadAll()
{
  defaultKeyTrans()->addKeyTrans();
  QStringList lst = KGlobal::dirs()->findAllResources("appdata", "*.keytab");

  for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it ) {
    KeyTrans* sc = KeyTrans::fromFile(*it);
    if (sc) sc->addKeyTrans();
  }
}

// Debugging material -----------------------------------------------------------
/*
void TestTokenizer(QBuffer &buf)
{
  // opening sequence

  buf.open(IO_ReadOnly);
  cc = buf.getch();
  lineno = 1;

  // Test tokenizer

  while (getSymbol(buf)) ReportToken();

  buf.close();
}

void test()
{
  // Opening sequence

  QCString txt =
#include "default.keytab.h"
  ;
  QBuffer buf(txt);
  if (0) TestTokenizer(buf);
  if (1) { KeyTrans kt; kt.scanTable(buf); }
}
*/
