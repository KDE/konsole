#ifndef KEYTRANS_H
#define KEYTRANS_H

#include <qstring.h>
#include <qlist.h>

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
