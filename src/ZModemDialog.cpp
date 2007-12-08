/*  This file is part of the KDE libraries
 *  Copyright (C) 2002 Waldo Bastian <bastian@kde.org>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License version 2 as published by the Free Software Foundation;
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 **/

// Own 
#include "ZModemDialog.h"

// Qt
#include <QtGui/QTextEdit>

// KDE
#include <KLocale>

using namespace Konsole;

ZModemDialog::ZModemDialog(QWidget *parent, bool modal, const QString &caption)
 : KDialog( parent )
{
  setObjectName( "zmodem_progress" );
  setModal( modal );
  setCaption( caption );
  setButtons( User1|Close );
  setButtonGuiItem( User1, KGuiItem(i18n("&Stop")) );

  setDefaultButton( User1 );
  setEscapeButton(User1);

  showButtonSeparator( true );
  enableButton(Close, false);
  _textEdit = new QTextEdit(this);
  _textEdit->setMinimumSize(400, 100);
  _textEdit->setReadOnly(true);
  setMainWidget(_textEdit);
  connect(this, SIGNAL(user1Clicked()), this, SLOT(slotClose()));
  connect(this,SIGNAL(closeClicked()),this,SLOT(slotClose()));
}

void ZModemDialog::addProgressText(const QString &txt)
{
  QTextCursor cursor = _textEdit->textCursor();

  cursor.insertBlock();
  cursor.insertText(txt);
}

void ZModemDialog::transferDone()
{
  enableButton(Close, true);
  enableButton(User1, false);
}

void ZModemDialog::slotClose()
{
  delayedDestruct();
  accept();
}

#include "ZModemDialog.moc"
