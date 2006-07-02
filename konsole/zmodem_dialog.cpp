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

#include "zmodem_dialog.h"

#include <q3textedit.h>

#include <klocale.h>

ZModemDialog::ZModemDialog(QWidget *parent, bool modal, const QString &caption)
 : KDialog( parent )
{
  setObjectName( "zmodem_progress" );
  setModal( modal );
  setCaption( caption );
  setButtons( User1|Close );
  setButtonGuiItem( User1, i18n("&Stop") );

  setDefaultButton( User1 );
  setEscapeButton(User1);

  showButtonSeparator( true );
  enableButton(Close, false);
  textEdit = new Q3TextEdit(this);
  textEdit->setMinimumSize(400, 100);
  setMainWidget(textEdit);
  connect(this, SIGNAL(user1Clicked()), this, SLOT(slotClose()));
}

void ZModemDialog::addProgressText(const QString &txt)
{
  int p = textEdit->paragraphs();
  textEdit->insertParagraph(txt, p);
}

void ZModemDialog::done()
{
  enableButton(Close, true);
  enableButton(User1, false);
}

void ZModemDialog::slotClose()
{
  delayedDestruct();
  accept();
}

#include "zmodem_dialog.moc"
