#ifndef KWRITED_H
#define KWRITED_H

#include <TEPty.h>
#include <qmultilineedit.h>

class KWrited : public QObject
{ Q_OBJECT
public:
  KWrited();
 ~KWrited();
private slots:
  void block_in(const char* bytes, int len);
private:
  QMultiLineEdit* wid;
  TEPty* pty;
};

#endif
