#ifndef SESSIONACTION_H
#define SESSIONACTION_H

#include <kaction.h>

class NewSessionAction : public KAction
{
  Q_OBJECT

public:
  NewSessionAction(const QObject *recvr = 0, const char *slot = 0, QObject *parent = 0);

  int plug( QWidget *w, int index = -1 );

  void setPopup( QPopupMenu *popup );

protected:

   QPopupMenu * m_popup;
  
};

#endif
