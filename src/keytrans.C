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

/*
   The keyboard translation table allows to configure konsoles behavior
   on key strokes.

   FIXME: some bug crept in, disallowing '\0' to be emitted.
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

#ifndef HERE
#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)
#endif

/* KeyEntry

   instances represent the individual assignments
*/

KeyTrans::KeyEntry::KeyEntry(int _ref, int _key, int _bits, int _mask, int _cmd, QString _txt)
: ref(_ref), key(_key), bits(_bits), mask(_mask), cmd(_cmd), txt(_txt)
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

/* KeyTrans

   combines the individual assignments to a proper map
   Takes part in a collection themself.
*/

KeyTrans::KeyTrans(const QString& path)
:m_hdr("")
,m_path(path)
,m_numb(0)
,m_fileRead(false)
{
  tableX.setAutoDelete(true);
  if (m_path=="[buildin]")
  {
     m_id = "default";
  }
  else
  {
     m_id = m_path;
     int i = m_id.findRev('/');
     if (i > -1)
        m_id = m_id.mid(i+1);
     i = m_id.findRev('.');
     if (i > -1)
        m_id = m_id.left(i);
  }
}

KeyTrans::KeyTrans()
{
/*  table.setAutoDelete(true);
  path = "";
  numb = 0;*/
}

KeyTrans::~KeyTrans()
{
}

KeyTrans::KeyEntry* KeyTrans::addEntry(int ref, int key, int bits, int mask, int cmd, QString txt)
// returns conflicting entry
{
  for (QListIterator<KeyEntry> it(tableX); it.current(); ++it)
  {
    if (it.current()->matches(key,bits,mask))
    {
      return it.current();
    }
  }
  tableX.append(new KeyEntry(ref,key,bits,mask,cmd,txt));
  return (KeyEntry*)NULL;
}

bool KeyTrans::findEntry(int key, int bits, int* cmd, const char** txt, int* len)
{
  if (!m_fileRead) readConfig();
  for (QListIterator<KeyEntry> it(tableX); it.current(); ++it)
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

// regular tokenizer
/* Tokens
   - Spaces
   - Name    (A-Za-z0-9)+
   - String
   - Opr     on of +-:
*/

#define SYMName    0
#define SYMString  1
#define SYMEol     2
#define SYMEof     3
#define SYMOpr     4
#define SYMError   5

#define inRange(L,X,H) ((L <= X) && (X <= H))
#define isNibble(X) (inRange('A',X,'F')||inRange('a',X,'f')||inRange('0',X,'9'))
#define convNibble(X) (inRange('0',X,'9')?X-'0':X+10-(inRange('A',X,'F')?'A':'a'))

class KeytabReader
{
public:
  KeytabReader(QString p, QIODevice &d);
public:
  void getCc();
  void getSymbol();
  void parseTo(KeyTrans* kt);
  void ReportError(const char* msg);
  void ReportToken(); // diagnostic
private:
  int     sym;
  QString res;
  int     len;
  int     slinno;
  int     scolno;
private:
  int     cc;
  int     linno;
  int     colno;
  QIODevice* buf;
  QString path;
};


KeytabReader::KeytabReader(QString p, QIODevice &d)
{
  path = p;
  buf = &d;
  cc = 0;
  colno = 0;
}

void KeytabReader::getCc()
{
  if (cc == '\n') { linno += 1; colno = 0; }
  if (cc < 0) return;
  cc = buf->getch();
  colno += 1;
}

void KeytabReader::getSymbol()
{
  res = ""; len = 0; sym = SYMError;
  while (cc == ' ') getCc(); // skip spaces
  if (cc == '#')      // skip comment
  {
    while (cc != '\n' && cc > 0) getCc();
  }
  slinno = linno;
  scolno = colno;
  if (cc <= 0)
  {
    sym = SYMEof; return; // eos
  }
  if (cc == '\n')
  {
    getCc();
    sym = SYMEol; return; // eol
  }
  if (inRange('A',cc,'Z')||inRange('a',cc,'z')||inRange('0',cc,'9'))
  {
    while (inRange('A',cc,'Z') || inRange('a',cc,'z') || inRange('0',cc,'9'))
    {
      res = res + (char)cc;
      getCc();
    }
    sym = SYMName;
    return;
  }
  if (strchr("+-:",cc))
  {
    res = "";
    res = res + (char)cc;
    getCc();
    sym = SYMOpr; return;
  }
  if (cc == '"')
  {
    getCc();
    while (cc >= ' ' && cc != '"')
    { int sc;
      if (cc == '\\') // handle quotation
      {
        getCc();
        switch (cc)
        {
          case 'E'  : sc = 27; getCc(); break;
          case 'b'  : sc =  8; getCc(); break;
          case 'f'  : sc = 12; getCc(); break;
          case 't'  : sc =  9; getCc(); break;
          case 'r'  : sc = 13; getCc(); break;
          case 'n'  : sc = 10; getCc(); break;
          case '\\' : // fall thru
          case '"'  : sc = cc; getCc(); break;
          case 'x'  : getCc();
                      sc = 0;
                      if (!isNibble(cc)) return; sc = 16*sc + convNibble(cc); getCc();
                      if (!isNibble(cc)) return; sc = 16*sc + convNibble(cc); getCc();
                      break;
          default   : return;
        }
      }
      else
      {
        // regular char
        sc = cc; getCc();
      }
      res = res + (char)sc;
      len = len + 1;
    }
    if (cc != '"') return;
    getCc();
    sym = SYMString; return;
  }
}

void KeytabReader::ReportToken() // diagnostic
{
  printf("sym(%d): ",slinno);
  switch(sym)
  {
    case SYMEol    : printf("End of line"); break;
    case SYMEof    : printf("End of file"); break;
    case SYMName   : printf("Name: %s",res.latin1()); break;
    case SYMOpr    : printf("Opr : %s",res.latin1()); break;
    case SYMString : printf("String len %d,%d ",res.length(),len);
                     for (unsigned i = 0; i < res.length(); i++)
                       printf(" %02x(%c)",res.latin1()[i],res.latin1()[i]>=' '?res.latin1()[i]:'?');
                     break;
  }
  printf("\n");
}

void KeytabReader::ReportError(const char* msg) // diagnostic
{
  fprintf(stderr,"%s(%d,%d):error: %s.\n",path.ascii(),slinno,scolno,msg);
}

// local symbol tables ---------------------------------------------------------------------

class KeyTransSymbols
{
public:
  KeyTransSymbols();
protected:
  void defOprSyms();
  void defModSyms();
  void defKeySyms();
  void defKeySym(const char* key, int val);
  void defOprSym(const char* key, int val);
  void defModSym(const char* key, int val);
public:
  QDict<QObject> keysyms;
  QDict<QObject> modsyms;
  QDict<QObject> oprsyms;
};

static KeyTransSymbols * syms = 0L;

// parser ----------------------------------------------------------------------------------
/* Syntax
   - Line :: [KeyName { ("+" | "-") ModeName } ":" (String|CommandName)] "\n"
   - Comment :: '#' (any but \n)*
*/

void KeyTrans::readConfig()
{
   if (m_fileRead) return;
   m_fileRead=true;
   QIODevice* buf(0);
   if (m_path=="[buildin]")
   {
      QCString txt =
#include "default.keytab.h"
;
      buf=new QBuffer(txt);
   }
   else
   {
      buf=new QFile(m_path);
   };
   KeytabReader ktr(m_path,*buf);
   ktr.parseTo(this);
   delete buf;
};

#define assertSyntax(Cond,Message) if (!(Cond)) { ReportError(Message); goto ERROR; }

void KeytabReader::parseTo(KeyTrans* kt)
{
  // Opening sequence

  buf->open(IO_ReadOnly);
  getCc();
  linno = 1;
  colno  = 1;
  getSymbol();

Loop:
  // syntax: ["key" KeyName { ("+" | "-") ModeName } ":" String/CommandName] ["#" Comment]
  if (sym == SYMName && !strcmp(res.latin1(),"keyboard"))
  {
    getSymbol(); assertSyntax(sym == SYMString, "Header expected")
    kt->m_hdr = i18n(res.latin1());
    getSymbol(); assertSyntax(sym == SYMEol, "Text unexpected")
    getSymbol();                   // eoln
    goto Loop;
  }
  if (sym == SYMName && !strcmp(res.latin1(),"key"))
  {
//printf("line %3d: ",startofsym);
    getSymbol(); assertSyntax(sym == SYMName, "Name expected")
    assertSyntax(syms->keysyms[res], "Unknown key name")
    int key = (int)syms->keysyms[res]-1;
//printf(" key %s (%04x)",res.latin1(),(int)syms->keysyms[res]-1);
    getSymbol(); // + - :
    int mode = 0;
    int mask = 0;
    while (sym == SYMOpr && (!strcmp(res.latin1(),"+") || !strcmp(res.latin1(),"-")))
    {
      bool on = !strcmp(res.latin1(),"+");
      getSymbol();
      // mode name
      assertSyntax(sym == SYMName, "Name expected")
      assertSyntax(syms->modsyms[res], "Unknown mode name")
      int bits = (int)syms->modsyms[res]-1;
      if (mask & (1 << bits))
      {
        fprintf(stderr,"%s(%d,%d): mode name used multible times.\n",path.ascii(),slinno,scolno);
      }
      else
      {
        mode |= (on << bits);
        mask |= (1 << bits);
      }
//printf(", mode %s(%d) %s",res.latin1(),(int)syms->modsyms[res]-1,on?"on":"off");
      getSymbol();
    }
    assertSyntax(sym == SYMOpr && !strcmp(res.latin1(),":"), "':' expected")
    getSymbol();
    // string or command
    assertSyntax(sym == SYMName || sym == SYMString,"Command or string expected")
    int cmd = 0;
    if (sym == SYMName)
    {
      assertSyntax(syms->oprsyms[res], "Unknown operator name")
      cmd = (int)syms->oprsyms[res]-1;
//printf(": do %s(%d)",res.latin1(),(int)syms->oprsyms[res]-1);
    }
    if (sym == SYMString)
    {
      cmd = CMD_send;
//printf(": send");
//for (unsigned i = 0; i < res.length(); i++)
//printf(" %02x(%c)",res.latin1()[i],res.latin1()[i]>=' '?res.latin1()[i]:'?');
    }
//printf(". summary %04x,%02x,%02x,%d\n",key,mode,mask,cmd);
    KeyTrans::KeyEntry* ke = kt->addEntry(slinno,key,mode,mask,cmd,res);
    if (ke)
    {
      fprintf(stderr,"%s(%d): keystroke already assigned in line %d.\n",path.ascii(),slinno,ke->ref);
    }
    getSymbol();
    assertSyntax(sym == SYMEol, "Unexpected text")
    goto Loop;
  }
  if (sym == SYMEol)
  {
    getSymbol();
    goto Loop;
  }

  assertSyntax(sym == SYMEof, "Undecodable Line")

  buf->close();
  return;

ERROR:
  while (sym != SYMEol && sym != SYMEof) getSymbol(); // eoln
  goto Loop;
}


// local symbol tables ---------------------------------------------------------------------
// material needed for parsing the config file.
// This is incomplete work.

void KeyTransSymbols::defKeySym(const char* key, int val)
{
  keysyms.insert(key,(QObject*)(val+1));
}

void KeyTransSymbols::defOprSym(const char* key, int val)
{
  oprsyms.insert(key,(QObject*)(val+1));
}

void KeyTransSymbols::defModSym(const char* key, int val)
{
  modsyms.insert(key,(QObject*)(val+1));
}

void KeyTransSymbols::defOprSyms()
{
  // Modifier
  defOprSym("scrollLineUp",  CMD_scrollLineUp  );
  defOprSym("scrollLineDown",CMD_scrollLineDown);
  defOprSym("scrollPageUp",  CMD_scrollPageUp  );
  defOprSym("scrollPageDown",CMD_scrollPageDown);
  defOprSym("emitSelection", CMD_emitSelection );
  defOprSym("prevSession",   CMD_prevSession   );
  defOprSym("nextSession",   CMD_nextSession   );
  defOprSym("newSession",    CMD_newSession    );
}

void KeyTransSymbols::defModSyms()
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

void KeyTransSymbols::defKeySyms()
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

static QIntDict<KeyTrans> * numb2keymap = 0L;

KeyTrans* KeyTrans::find(int numb)
{
  KeyTrans* res = numb2keymap->find(numb);
  return res ? res : numb2keymap->find(0);
}

KeyTrans* KeyTrans::find(const QString &id)
{
  QIntDictIterator<KeyTrans> it(*numb2keymap);
  while(it.current())
  {
    if (it.current()->id() == id)
       return it.current();
    ++it;
  }
  return numb2keymap->find(0);
}

int KeyTrans::count()
{
  return numb2keymap->count();
}

void KeyTrans::addKeyTrans()
{
  m_numb = keytab_serial ++;
  numb2keymap->insert(m_numb,this);
}

void KeyTrans::loadAll()
{
  if (!numb2keymap)
    numb2keymap = new QIntDict<KeyTrans>;
  if (!syms)
    syms = new KeyTransSymbols;

  //defaultKeyTrans()->addKeyTrans();
  KeyTrans* sc = new KeyTrans("[buildin]");
  sc->addKeyTrans();

  QStringList lst = KGlobal::dirs()->findAllResources("appdata", "*.keytab");

  for(QStringList::Iterator it = lst.begin(); it != lst.end(); ++it )
  {
    //QFile file(QFile::encodeName(*it));
    sc = new KeyTrans(QFile::encodeName(*it));
    //KeyTrans* sc = KeyTrans::fromDevice(QFile::encodeName(*it),file);
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
