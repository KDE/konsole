// This module is work in progress.

/*
   State is that TEMuVt102 already gets it's entries from
   the table in here. Following section is pretty complete,
   to. What is left to do is, to create the table from a file,
   and to put things more straight. The remainder of the
   file contains yet incomplete work towards this.
*/

#include "keytrans.h"
#include <qnamespace.h>

#include <stdio.h>

// KeyEntry -

KeyTrans::KeyEntry::KeyEntry(int _key, int _bits, int _mask, int _cmd, const char* _txt, int _len)
: key(_key), bits(_bits), mask(_mask), cmd(_cmd), txt(_txt), len(_len)
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

void KeyTrans::addEntry(int key, int bits, int mask, int cmd, const char* txt, int len)
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
  table.append(new KeyEntry(key,bits,mask,cmd,txt,len));
}

void KeyTrans::addEntry(int key, int maskedbits, int cmd, const char* txt, int len)
{
  // we come in with tristate encoded bits here and
  // separate a mask and a value from it.
  // FIXME: this routine is of temporary use only.
  int mask = 0;
  int bits = 0;
  for (int bit = 0; bit < BITS_COUNT; bit++)
  {
    mask |= (((maskedbits&(1 << (2*bit+1))) != 0) << bit);
    bits |= (((maskedbits&(1 << (2*bit+0))) != 0) << bit);
  }
//printf("KEY: %d, bits:0x%02x, mask:0x%02x (masked bits:0x%02x)\n",key,bits,mask,maskedbits);
  addEntry(key,bits,mask,cmd,txt,len);
}

bool KeyTrans::findEntry(int key, int bits, int* cmd, const char** txt, int* len)
{
  for (QListIterator<KeyEntry> it(table); it.current(); ++it)
    if (it.current()->matches(key,bits,0xffff))
    {
      *cmd = it.current()->cmd;
      *txt = it.current()->txt;
      *len = it.current()->len;
      return TRUE;
    }
  return FALSE;
}

// ----------------------------------------
// ----------------------------------------
// ----------------------------------------

// The following is supposed to be read in from a file.

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

#define bOn(X)  ((3<<(2*X)))
#define bOff(X) ((2<<(2*X)))

void KeyTrans::addXtermKeys()
{
  addEntry(Qt::Key_Escape, 0, CMD_send, "\033", 1);
  addEntry(Qt::Key_Tab   , 0, CMD_send, "\t", 1);

// VT100 can add an extra \n after return.
// The NewLine mode is set by an escape sequence.

  addEntry(Qt::Key_Return, bOff(BITS_NewLine), CMD_send, "\r", 1);
  addEntry(Qt::Key_Return, bOn(BITS_NewLine), CMD_send, "\r\n", 2);

// Some desperately try to save the ^H.
// The BsHack mode is set by regular
// configurations means for convenience.

  addEntry(Qt::Key_Backspace, bOff(BITS_BsHack), CMD_send, "\x08", 1);
  addEntry(Qt::Key_Delete   , bOff(BITS_BsHack), CMD_send, "\x7f", 1);

  addEntry(Qt::Key_Backspace, bOn(BITS_BsHack), CMD_send, "\x7f", 1);
  addEntry(Qt::Key_Delete   , bOn(BITS_BsHack), CMD_send, "\033[3~", 4);

// These codes are for the VT52 mode of VT100
// The Ansi mode (i.e. VT100 mode) is set by
// an escape sequence

  addEntry(Qt::Key_Up   , bOff(BITS_Shift) | bOff(BITS_Ansi), CMD_send, "\033A", 2);
  addEntry(Qt::Key_Down , bOff(BITS_Shift) | bOff(BITS_Ansi), CMD_send, "\033B", 2);
  addEntry(Qt::Key_Right, bOff(BITS_Ansi), CMD_send, "\033C", 2);
  addEntry(Qt::Key_Left , bOff(BITS_Ansi), CMD_send, "\033D", 2);

// VT100 emits a mode bit together
// with the arrow keys.The AppCuKeys
// mode is set by an escape sequence.

  addEntry(Qt::Key_Up   , bOff(BITS_Shift) | bOn(BITS_Ansi) | bOn(BITS_AppCuKeys), CMD_send, "\033OA", 3);
  addEntry(Qt::Key_Down , bOff(BITS_Shift) | bOn(BITS_Ansi) | bOn(BITS_AppCuKeys), CMD_send, "\033OB", 3);
  addEntry(Qt::Key_Right, bOn(BITS_Ansi) | bOn(BITS_AppCuKeys), CMD_send, "\033OC", 3);
  addEntry(Qt::Key_Left , bOn(BITS_Ansi) | bOn(BITS_AppCuKeys), CMD_send, "\033OD", 3);

  addEntry(Qt::Key_Up   , bOff(BITS_Shift) | bOn(BITS_Ansi) | bOff(BITS_AppCuKeys), CMD_send, "\033[A", 3);
  addEntry(Qt::Key_Down , bOff(BITS_Shift) | bOn(BITS_Ansi) | bOff(BITS_AppCuKeys), CMD_send, "\033[B", 3);
  addEntry(Qt::Key_Right, bOn(BITS_Ansi) | bOff(BITS_AppCuKeys), CMD_send, "\033[C", 3);
  addEntry(Qt::Key_Left , bOn(BITS_Ansi) | bOff(BITS_AppCuKeys), CMD_send, "\033[D", 3);

// linux functions keys F1 bOff(BITS_F5 differ from xterm
//
// F1, CMD_send, "\033[[A" 
// F2, CMD_send, "\033[[B" 
// F3, CMD_send, "\033[[C" 
// F4, CMD_send, "\033[[D" 
// F5, CMD_send, "\033[[E" 

// function keys

  addEntry(Qt::Key_F1    , 0, CMD_send, "\033[11~", 5);
  addEntry(Qt::Key_F2    , 0, CMD_send, "\033[12~", 5);
  addEntry(Qt::Key_F3    , 0, CMD_send, "\033[13~", 5);
  addEntry(Qt::Key_F4    , 0, CMD_send, "\033[14~", 5);
  addEntry(Qt::Key_F5    , 0, CMD_send, "\033[15~", 5);
  addEntry(Qt::Key_F6    , 0, CMD_send, "\033[17~", 5);
  addEntry(Qt::Key_F7    , 0, CMD_send, "\033[18~", 5);
  addEntry(Qt::Key_F8    , 0, CMD_send, "\033[19~", 5);
  addEntry(Qt::Key_F9    , 0, CMD_send, "\033[20~", 5);
  addEntry(Qt::Key_F10   , 0, CMD_send, "\033[21~", 5);
  addEntry(Qt::Key_F11   , 0, CMD_send, "\033[23~", 5);
  addEntry(Qt::Key_F12   , 0, CMD_send, "\033[24~", 5);

  addEntry(Qt::Key_Home  , 0, CMD_send, "\033[H", 3);
  addEntry(Qt::Key_End   , 0, CMD_send, "\033[F", 3);
  addEntry(Qt::Key_Insert, bOff(BITS_Shift), CMD_send, "\033[2~", 4);

  addEntry(Qt::Key_Prior , bOff(BITS_Shift), CMD_send, "\033[5~", 4);
  addEntry(Qt::Key_Next  , bOff(BITS_Shift), CMD_send, "\033[6~", 4);

// Keypad bOff(BITS_Enter. See comment on Return above.

  addEntry(Qt::Key_Enter,  bOn(BITS_NewLine), CMD_send, "\r\n", 2);
  addEntry(Qt::Key_Enter,  bOff(BITS_NewLine), CMD_send, "\r", 1);

// Experimental: add commands
  addEntry(Qt::Key_Up,     bOn(BITS_Shift), CMD_scrollLineUp, "", 0);
  addEntry(Qt::Key_Down,   bOn(BITS_Shift), CMD_scrollLineDown, "", 0);
  addEntry(Qt::Key_Prior,  bOn(BITS_Shift), CMD_scrollPageUp, "", 0);
  addEntry(Qt::Key_Next,   bOn(BITS_Shift), CMD_scrollPageDown, "", 0);
  addEntry(Qt::Key_Insert, bOn(BITS_Shift), CMD_emitSelection, "", 0);

  addEntry(Qt::Key_Space,  bOn(BITS_Control), CMD_send, "\x00", 1); // ctrl-@ =^= ctrl-space

// FIXME: QT keypad stuff is broken.

// Other strings are emitted by konsole, too.
}

/* ------------------------------------------------------------------------- */
/*                                                                           */
/* Scanner for keyboard configuration                                        */
/*                                                                           */
/* ------------------------------------------------------------------------- */

// material needed for parsing the config file.
// This is incomplete work.


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
  // syntax: [KeyName { ("+" | "-") ModeName } ":" String/Command] ["#" Comment]
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

/*Here the file as it is supposed to be

# [KeyTab] Konsole Keyboard Table
#
# This configuration table allows to customize the
# meaning of the keys.
#
# If the key is not found here, the text of the
# key event as provided by QT is emitted, possibly
# preceeded by ESC if the Alt key is pressed.

key Escape:"\E"
key Tab   :"\t"

# VT100 can add an extra \n after return.
# The NewLine mode is set by an escape sequence.

key Return-NewLine:"\r"  
key Return+NewLine:"\r\n"

# Some desperately try to save the ^H.
# The BsHack mode is set by regular
# configurations means for convenience.

key Backspace-BsHack:"\x08"
key Delete   -BsHack:"\x7f"

key Backspace+BsHack:"\x7f"
key Delete   +BsHack:"\E[3~"

# These codes are for the VT52 mode of VT100
# The Ansi mode (i.e. VT100 mode) is set by
# an escape sequence

key Up   -Shift-Ansi:"\EA"
key Down -Shift-Ansi:"\EB"
key Right-Ansi:"\EC"
key Left -Ansi:"\ED"

# VT100 emits a mode bit together
# with the arrow keys.The AppCuKeys
# mode is set by an escape sequence.

key Up   -Shift+Ansi+AppCuKeys:"\EOA"
key Down -Shift+Ansi+AppCuKeys:"\EOB"
key Right+Ansi+AppCuKeys:"\EOC"
key Left +Ansi+AppCuKeys:"\EOD"

key Up   -Shift+Ansi-AppCuKeys:"\E[A"
key Down -Shift+Ansi-AppCuKeys:"\E[B"
key Right+Ansi-AppCuKeys:"\E[C"
key Left +Ansi-AppCuKeys:"\E[D"

# linux functions keys F1-F5 differ from xterm
#
# F1:"\E[[A" 
# F2:"\E[[B" 
# F3:"\E[[C" 
# F4:"\E[[D" 
# F5:"\E[[E" 

# function keys

key F1    :"\E[11~"
key F2    :"\E[12~"
key F3    :"\E[13~"
key F4    :"\E[14~"
key F5    :"\E[15~"
key F6    :"\E[17~" 
key F7    :"\E[18~" 
key F8    :"\E[19~" 
key F9    :"\E[20~" 
key F10   :"\E[21~" 
key F11   :"\E[23~" 
key F12   :"\E[24~" 

key Home  :"\E[H"  
key End   :"\E[F"  
key Prior -Shift:"\E[5~"  
key Next  -Shift:"\E[6~"  
key Insert-Shift:"\E[2~"  

# Keypad-Enter. See comment on Return above.

key Enter+NewLine:"\r\n"
key Enter-NewLine:"\r"  

key Space +Control:"\x00"

# some of keys are used by konsole.

key Up    +Shift  : scrollUpLine
key Prior +Shift  : scrollUpPage
key Down  +Shift  : scrollDownLine
key Next  +Shift  : scrollDownPage
key Insert+Shift  : emitSelection
key Print +Control: answerBack

#----------------------------------------------------------

# keypad characters as offered by Qt
# cannot be recognized as such.

#----------------------------------------------------------

# Following other strings as emitted by konsole.

*/
