#ifndef KEYTRANS_H
#define KEYTRANS_H

#include <qstring.h>
#include <qlist.h>

#define BITS_NewLine    0
#define BITS_BsHack     1
#define BITS_Ansi       2
#define BITS_AppCuKeys  3
#define BITS_Control    4
#define BITS_Shift      5
#define BITS_Alt        6
#define BITS_COUNT      7

#define CMD_send           0
#define CMD_emitSelection  1
#define CMD_scrollPageUp   2
#define CMD_scrollPageDown 3
#define CMD_scrollLineUp   4
#define CMD_scrollLineDown 5

#define BITS(x,v) ((((v)!=0)<<(x)))

class KeyTrans
{
public:
  KeyTrans();
  ~KeyTrans();
public: // put somewhere else
  void addXtermKeys();
public:
  void addEntry(int key, int maskedbits, int cmd, const char* txt, int len);
  bool findEntry(int key, int bits, int* cmd, const char** txt, int* len);
private:
  void addEntry(int key, int bits, int mask, int cmd, const char* txt, int len);
  class KeyEntry
  {
  public:
    KeyEntry(int key, int bits, int mask, int cmd, const char* txt, int len);
    ~KeyEntry();
  public:
    bool matches(int key, int bits, int mask);
    QString text();
  private:
    int     key;
    int     bits;
    int     mask;
  public:
    int cmd;
    const char* txt;
    int len;
  };
private:
  QList<KeyEntry> table;
};

#endif
