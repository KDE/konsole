#include "keytrans.h"
#include <qnamespace.h>

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
      // reject e, with error message
      return;
    }
  }
  table.append(new KeyEntry(key,bits,mask,txt));
}

void KeyTrans::addEntry(int key, int maskedbits, QString txt)
{
  // we come in with tristate encoded bits here and
  // separate a mask and a value from it.

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

#define BITS_NewLine    0
#define BITS_BsHack     1
#define BITS_Ansi       2
#define BITS_AppCuKeys  4

#define bOn(x)    ((3<<(2*x)))
#define bOff(x)   ((2<<(2*x)))

void KeyTrans::addXtermKeys()
{
  addEntry(Qt::Key_Escape, 0, "\033");
  addEntry(Qt::Key_Tab   , 0, "\t");

// VT100 can add an extra \n after return.
// The NewLine mode is set by an escape sequence.

  addEntry(Qt::Key_Return, -BITS_NewLine, "\r");
  addEntry(Qt::Key_Return, +BITS_NewLine, "\r\n");

// Some desperately try to save the ^H.
// The BsHack mode is set by regular
// configurations means for convenience.

  addEntry(Qt::Key_Backspace, -BITS_BsHack, "\x08");
  addEntry(Qt::Key_Delete   , -BITS_BsHack, "\x7f");

  addEntry(Qt::Key_Backspace, +BITS_BsHack, "\x7f");
  addEntry(Qt::Key_Delete   , +BITS_BsHack, "\033[3~");

// These codes are for the VT52 mode of VT100
// The Ansi mode (i.e. VT100 mode) is set by
// an escape sequence

  addEntry(Qt::Key_Up   , -BITS_Ansi, "\033A");
  addEntry(Qt::Key_Down , -BITS_Ansi, "\033B");
  addEntry(Qt::Key_Right, -BITS_Ansi, "\033C");
  addEntry(Qt::Key_Left , -BITS_Ansi, "\033D");

// VT100 emits a mode bit together
// with the arrow keys.The AppCuKeys
// mode is set by an escape sequence.

  addEntry(Qt::Key_Up   , +BITS_Ansi +BITS_AppCuKeys, "\033OA");
  addEntry(Qt::Key_Down , +BITS_Ansi +BITS_AppCuKeys, "\033OB");
  addEntry(Qt::Key_Right, +BITS_Ansi +BITS_AppCuKeys, "\033OC");
  addEntry(Qt::Key_Left , +BITS_Ansi +BITS_AppCuKeys, "\033OD");

  addEntry(Qt::Key_Up   , +BITS_Ansi -BITS_AppCuKeys, "\033[A");
  addEntry(Qt::Key_Down , +BITS_Ansi -BITS_AppCuKeys, "\033[B");
  addEntry(Qt::Key_Right, +BITS_Ansi -BITS_AppCuKeys, "\033[C");
  addEntry(Qt::Key_Left , +BITS_Ansi -BITS_AppCuKeys, "\033[D");

// linux functions keys F1 -BITS_F5 differ from xterm
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

// Keypad -BITS_Enter. See comment on Return above.

  addEntry(Qt::Key_Enter,  +BITS_NewLine, "\r\n");
  addEntry(Qt::Key_Enter,  -BITS_NewLine, "\r");

// FIXME: get keypad somehow
//Space +BITS_Control, "\x00");
//Print +BITS_Control, "");

// Other strings are emitted by konsole, too.
}
