#ifndef KEYTRANS_H
#define KEYTRANS_H

#include <qstring.h>
#include <qlist.h>

#define BITS_NewLine    0
#define BITS_BsHack     1
#define BITS_Ansi       2
#define BITS_AppCuKeys  3
#define BITS_COUNT      4
//FIXME: add Shift,Alt,Control

#define MASKEDBITS_On(x)  ((3<<(2*x)))
#define MASKEDBITS_Off(x) ((2<<(2*x)))
#define BITS(x,v)         ((((v)!=0)<<(x)))

class KeyTrans
{
public:
  KeyTrans();
  ~KeyTrans();
public: // put somewhere else
  void addXtermKeys();
public:
  void addEntry(int key, int maskedbits, QString txt);
  QString findEntry(int key, int bits);
private:
  void addEntry(int key, int bits, int mask, QString txt);
  class KeyEntry
  {
  public:
    KeyEntry(int key, int bits, int mask, QString sequence);
    ~KeyEntry();
  public:
    bool matches(int key, int bits, int mask);
    QString text();
  private:
    int     key;
    int     bits;
    int     mask;
    QString txt;
  };
private:
  QList<KeyEntry> table;
};

#endif
