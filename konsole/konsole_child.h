/* ----------------------------------------------------------------------- *
 * [konsole_child.h]             Konsole                                   *
 * ----------------------------------------------------------------------- *
 * This file is part of Konsole, an X terminal.                            *
 *                                                                         *
 * Copyright (c) 2002 by Stephan Binner <binner@kde.org>                   *
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef KONSOLE_CHILD_H
#define KONSOLE_CHILD_H

#include <kaction.h>
#include <klocale.h>
#include <kmainwindow.h>

#include "TEWidget.h"
#include "schema.h"
#include "session.h"

#include <krootpixmap.h>
#include <kwinmodule.h>

class KonsoleChild : public KMainWindow
{ Q_OBJECT

public:
  KonsoleChild(TESession*, int columns, int lines, int scrollbar_location, int frame_style,
               ColorSchema* schema,QFont font, int bellmode, QString wordcharacters,
               bool blinkingCursor, bool ctrlDrag, bool terminalSizeHint, int lineSpacing,
	       bool cutToBeginningOfLine, bool allowResize);
  void run();
  ~KonsoleChild();
  TESession*  session() { return se; }

signals:
  void doneChild(KonsoleChild*, TESession*);

private slots:
  void configureRequest(TEWidget*,int,int,int);
  void doneSession(TESession*);
  void updateTitle();
  void slotRenameSession(TESession* ses, const QString &name);
  void restoreAllListenToKeyPress();
  void changeColumns(int columns);

  void currentDesktopChanged(int desk);
  void slotBackgroundChanged(int desk);

  void sendSignal(int sn);
  void attachSession();
  void renameSession();
  void closeSession();

private:
  void setColLin(int columns, int lines);
  void pixmap_menu_activated(int item,QString pmPath);

  bool session_terminated;
  bool session_transparent;
  int  wallpaperSource;

  TESession*     se;
  bool allowResize;
  TEWidget*      te;
  KPopupMenu*    m_rightButton;
  KRootPixmap*   rootxpm;
  KWinModule*    kWinModule;
 };

#endif
