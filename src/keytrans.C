// This module is work in progress.

#include "keytrans.h"
#include <qnamespace.h>

#include <stdio.h>

// KeyEntry -

KeyTrans::KeyEntry::KeyEntry(int _key, int _bits, int _mask, QString _txt)
: key(_key), bits(_bits), mask(_mask), txt(_txt)
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
}

KeyTrans::~KeyTrans()
{
}

void KeyTrans::addEntry(int key, int bits, int mask, QString txt)
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
  table.append(new KeyEntry(key,bits,mask,txt));
}

void KeyTrans::addEntry(int key, int maskedbits, QString txt)
{
  // we come in with tristate encoded bits here and
  // separate a mask and a value from it.
  // A bit clumpsy, isn't it?
  int mask = 0;
  int bits = 0;
  for (int bit = 0; bit < BITS_COUNT; bit++)
  {
    mask |= (((maskedbits&(1 << (2*bit+1))) != 0) << bit);
    bits |= (((maskedbits&(1 << (2*bit+0))) != 0) << bit);
  }
//printf("KEY: %d, bits:0x%02x, mask:0x%02x (masked bits:0x%02x)\n",key,bits,mask,maskedbits);
  addEntry(key,bits,mask,txt);
}

QString KeyTrans::findEntry(int key, int bits)
{
  for (QListIterator<KeyEntry> it(table); it.current(); ++it)
    if (it.current()->matches(key,bits,0xffff))
      return it.current()->text();
  return QString();
}

// ----------------------------------------

// The following is supposed to be read in from a file, too.

// [KeyTab] Konsole Keyboard Table
//
// This configuration table allows to customize the
// meaning of function (gray) keys.
//
// The regular keys are considered to represent
// regular characters, though things are a little
// more complicate with them. Especially, Shift,
// Ctrl and Alt modifies these keys to exhibit
// function codes.
//
// FIXME: we have to fit this somehow into the schema.
// It would also be nice if we could mention all other
// codes that are emitted by konsole here. These are
// replies and events exposed due to the protocol.
// Some key combinations are hardcoded within konsole
// and cannot be modified without recompiling.
//
// These are:
//
// Up   +Shift: scroll History Buffer up   one line
// Prior+Shift: scroll History Buffer up   one page
// Down +Shift: scroll History Buffer down one line
// Next +Shift: scroll History Buffer down one page
//

#define bOn(X)  MASKEDBITS_On(X)
#define bOff(X) MASKEDBITS_Off(X)

void KeyTrans::addXtermKeys()
{
  addEntry(Qt::Key_Escape, 0, "\033");
  addEntry(Qt::Key_Tab   , 0, "\t");

// VT100 can add an extra \n after return.
// The NewLine mode is set by an escape sequence.

  addEntry(Qt::Key_Return, bOff(BITS_NewLine), "\r");
  addEntry(Qt::Key_Return, bOn(BITS_NewLine), "\r\n");

// Some desperately try to save the ^H.
// The BsHack mode is set by regular
// configurations means for convenience.

  addEntry(Qt::Key_Backspace, bOff(BITS_BsHack), "\x08");
  addEntry(Qt::Key_Delete   , bOff(BITS_BsHack), "\x7f");

  addEntry(Qt::Key_Backspace, bOn(BITS_BsHack), "\x7f");
  addEntry(Qt::Key_Delete   , bOn(BITS_BsHack), "\033[3~");

// These codes are for the VT52 mode of VT100
// The Ansi mode (i.e. VT100 mode) is set by
// an escape sequence

  addEntry(Qt::Key_Up   , bOff(BITS_Ansi), "\033A");
  addEntry(Qt::Key_Down , bOff(BITS_Ansi), "\033B");
  addEntry(Qt::Key_Right, bOff(BITS_Ansi), "\033C");
  addEntry(Qt::Key_Left , bOff(BITS_Ansi), "\033D");

// VT100 emits a mode bit together
// with the arrow keys.The AppCuKeys
// mode is set by an escape sequence.

  addEntry(Qt::Key_Up   , bOn(BITS_Ansi) | bOn(BITS_AppCuKeys), "\033OA");
  addEntry(Qt::Key_Down , bOn(BITS_Ansi) | bOn(BITS_AppCuKeys), "\033OB");
  addEntry(Qt::Key_Right, bOn(BITS_Ansi) | bOn(BITS_AppCuKeys), "\033OC");
  addEntry(Qt::Key_Left , bOn(BITS_Ansi) | bOn(BITS_AppCuKeys), "\033OD");

  addEntry(Qt::Key_Up   , bOn(BITS_Ansi) | bOff(BITS_AppCuKeys), "\033[A");
  addEntry(Qt::Key_Down , bOn(BITS_Ansi) | bOff(BITS_AppCuKeys), "\033[B");
  addEntry(Qt::Key_Right, bOn(BITS_Ansi) | bOff(BITS_AppCuKeys), "\033[C");
  addEntry(Qt::Key_Left , bOn(BITS_Ansi) | bOff(BITS_AppCuKeys), "\033[D");

// linux functions keys F1 bOff(BITS_F5 differ from xterm
//
// F1, "\033[[A" 
// F2, "\033[[B" 
// F3, "\033[[C" 
// F4, "\033[[D" 
// F5, "\033[[E" 

// function keys

  addEntry(Qt::Key_F1    , 0, "\033[11~");
  addEntry(Qt::Key_F2    , 0, "\033[12~");
  addEntry(Qt::Key_F3    , 0, "\033[13~");
  addEntry(Qt::Key_F4    , 0, "\033[14~");
  addEntry(Qt::Key_F5    , 0, "\033[15~");
  addEntry(Qt::Key_F6    , 0, "\033[17~");
  addEntry(Qt::Key_F7    , 0, "\033[18~");
  addEntry(Qt::Key_F8    , 0, "\033[19~");
  addEntry(Qt::Key_F9    , 0, "\033[20~");
  addEntry(Qt::Key_F10   , 0, "\033[21~");
  addEntry(Qt::Key_F11   , 0, "\033[23~");
  addEntry(Qt::Key_F12   , 0, "\033[24~");

  addEntry(Qt::Key_Home  , 0, "\033[H");
  addEntry(Qt::Key_End   , 0, "\033[F");
  addEntry(Qt::Key_Prior , 0, "\033[5~");
  addEntry(Qt::Key_Next  , 0, "\033[6~");
  addEntry(Qt::Key_Insert, 0, "\033[2~");

// Keypad bOff(BITS_Enter. See comment on Return above.

  addEntry(Qt::Key_Enter,  bOn(BITS_NewLine), "\r\n");
  addEntry(Qt::Key_Enter,  bOff(BITS_NewLine), "\r");

// FIXME: get keypad somehow
//Space bOn(BITS_Control, "\x00");
//Print bOn(BITS_Control, "");

// Other strings are emitted by konsole, too.
}

// material needed for parsing the config file.

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Scanner for keyboard configuration                                        */
/*                                                                           */
/* ------------------------------------------------------------------------- */

void defKeySym(const char*, int)
{
}

void defModSym(const char*, int)
{
}

void defModSyms()
{
  // Modifier
  defModSym("Shift",      Qt::SHIFT         );
  defModSym("Ctrl",       Qt::CTRL          );
  defModSym("Alt",        Qt::ALT           );
  // Modes
  defModSym("BsHack",     BITS_BsHack       ); // deprecated
  defModSym("Ansi",       BITS_Ansi         );
  defModSym("NewLine",    BITS_NewLine      );
  defModSym("AppCuKeys",  BITS_AppCuKeys    );
}

void defKeySyms()
{
  defKeySym("Escape",     Qt::Key_Escape    );
  defKeySym("Tab",        Qt::Key_Tab       );
  defKeySym("Backtab",    Qt::Key_Backtab   );
  defKeySym("Backspace",  Qt::Key_Backspace );
  defKeySym("Return",     Qt::Key_Return    );
  defKeySym("Enter",      Qt::Key_Enter     );
  defKeySym("Insert",     Qt::Key_Insert    );
  defKeySym("Delete",     Qt::Key_Delete    );
  defKeySym("Pause",      Qt::Key_Pause     );
  defKeySym("Print",      Qt::Key_Print     );
  defKeySym("SysReq",     Qt::Key_SysReq    );
  defKeySym("Home",       Qt::Key_Home      );
  defKeySym("End",        Qt::Key_End       );
  defKeySym("Left",       Qt::Key_Left      );
  defKeySym("Up",         Qt::Key_Up        );
  defKeySym("Right",      Qt::Key_Right     );
  defKeySym("Down",       Qt::Key_Down      );
  defKeySym("Prior",      Qt::Key_Prior     );
  defKeySym("Next",       Qt::Key_Next      );
  defKeySym("Shift",      Qt::Key_Shift     );
  defKeySym("Control",    Qt::Key_Control   );
  defKeySym("Meta",       Qt::Key_Meta      );
  defKeySym("Alt",        Qt::Key_Alt       );
  defKeySym("CapsLock",   Qt::Key_CapsLock  );
  defKeySym("NumLock",    Qt::Key_NumLock   );
  defKeySym("ScrollLock", Qt::Key_ScrollLock);
  defKeySym("F1",         Qt::Key_F1        );
  defKeySym("F2",         Qt::Key_F2        );
  defKeySym("F3",         Qt::Key_F3        );
  defKeySym("F4",         Qt::Key_F4        );
  defKeySym("F5",         Qt::Key_F5        );
  defKeySym("F6",         Qt::Key_F6        );
  defKeySym("F7",         Qt::Key_F7        );
  defKeySym("F8",         Qt::Key_F8        );
  defKeySym("F9",         Qt::Key_F9        );
  defKeySym("F10",        Qt::Key_F10       );
  defKeySym("F11",        Qt::Key_F11       );
  defKeySym("F12",        Qt::Key_F12       );
  defKeySym("F13",        Qt::Key_F13       );
  defKeySym("F14",        Qt::Key_F14       );
  defKeySym("F15",        Qt::Key_F15       );
  defKeySym("F16",        Qt::Key_F16       );
  defKeySym("F17",        Qt::Key_F17       );
  defKeySym("F18",        Qt::Key_F18       );
  defKeySym("F19",        Qt::Key_F19       );
  defKeySym("F20",        Qt::Key_F20       );
  defKeySym("F21",        Qt::Key_F21       );
  defKeySym("F22",        Qt::Key_F22       );
  defKeySym("F23",        Qt::Key_F23       );
  defKeySym("F24",        Qt::Key_F24       );
  defKeySym("F25",        Qt::Key_F25       );
  defKeySym("F26",        Qt::Key_F26       );
  defKeySym("F27",        Qt::Key_F27       );
  defKeySym("F28",        Qt::Key_F28       );
  defKeySym("F29",        Qt::Key_F29       );
  defKeySym("F30",        Qt::Key_F30       );
  defKeySym("F31",        Qt::Key_F31       );
  defKeySym("F32",        Qt::Key_F32       );
  defKeySym("F33",        Qt::Key_F33       );
  defKeySym("F34",        Qt::Key_F34       );
  defKeySym("F35",        Qt::Key_F35       );
  defKeySym("Super_L",    Qt::Key_Super_L   );
  defKeySym("Super_R",    Qt::Key_Super_R   );
  defKeySym("Menu",       Qt::Key_Menu      );
  defKeySym("Hyper_L",    Qt::Key_Hyper_L   );
  defKeySym("Hyper_R",    Qt::Key_Hyper_R   );
}

// [scant.c]

#include <stdio.h>

// We define a very poor scanner, here.

#define skipspaces while (*cc == ' ') cc++

#define skipname \
    id = cc; \
    while ('A' <= *cc && *cc <= 'Z' || \
           'a' <= *cc && *cc <= 'z' || \
           '0' <= *cc && *cc <= '9') cc++

#define skipchar(C) if (*cc == (C)) cc++; else { printf("error: expecting '%c' at %s.\n",C,cc); return; }

#define skiphex() cc++

#define FALSE 0
#define TRUE 1

#define HERE printf("%s(%d): here\n",__FILE__,__LINE__)

static void scanline(char* cc)
{ char* id;
Loop:
  // syntax: [KeyName { ("+" | "-") ModeName } ":" String] ["#" Comment]
  skipspaces;
  if ('A' <= *cc && *cc <= 'Z')
  {
    skipname;
    printf("key: >%.*s<\n",cc-id,id);
    skipspaces;
    while (*cc == '+' || *cc == '-')
    { bool opt_on = (*cc++ == '+');
      skipspaces;
      skipname;
      printf("mode: >%.*s< %s\n",cc-id,id,opt_on?"on":"off");
      skipspaces;
    }
    skipchar(':')
    skipspaces;
    // scanstring
    skipchar('"')
    while (*cc)
    {
      if (*cc < ' ') break;
      if (*cc == '"') break;
      if (*cc != '\\') { cc++; continue; }
      cc++;
      if (!*cc)  { printf("error: unexpected end of string.\n"); return; }
      switch (*cc)
      {
        case '\\' :
        case 'n'  :
        case 'r'  :
        case 'f'  :
        case 'b'  :
        case 't'  :
        case '"'  :
        case 'E'  : cc++; break;
        case 'u'  : cc++; skiphex(); skiphex(); skiphex(); skiphex(); break;
        case 'x'  : cc++; skiphex(); skiphex(); break;
        default   : printf("error: invalid char '%c' after \\.\n",*cc); return;
      }
    }
    skipchar('"')
    skipspaces;
  }
  if (*cc == '#')
  {
    // Skip Comment
    while (*cc && *cc != '\n') cc++;
  }
  if (*cc == '\n') { cc++; goto Loop; }
  if (*cc)
  {
    printf("error: invalid character '%c' at %s.\n",*cc,cc); return;
  }

}

/*
int main()
{
  scanline
  (
#include "KeyTab.sys"
  );
  return 0;
}
*/
