#ifndef KWRITED_H
#define KWRITED_H

#include <TEPty.h>
#include <qtextedit.h>

class KWrited : public QObject
{ Q_OBJECT
public:
  KWrited();
 ~KWrited();
private slots:
  void block_in(int fd);
private:
  QTextEdit* wid;
  TEPty* pty;
};

#endif
