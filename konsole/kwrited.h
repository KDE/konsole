#ifndef KWRITED_H
#define KWRITED_H

#include <qtextedit.h>

class KPty;

class KWrited : public QObject
{ Q_OBJECT
public:
  KWrited();
 ~KWrited();
private slots:
  void block_in(int fd);
private:
  QTextEdit* wid;
  KPty* pty;
};

#endif
