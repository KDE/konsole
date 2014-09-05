/*  This file is part of the KDE libraries
 *  Copyright 2002 Waldo Bastian <bastian@kde.org>
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

// KDE
#include <KLocalizedString>
#include <KTextEdit>

using Konsole::ZModemDialog;

ZModemDialog::ZModemDialog(QWidget* aParent, bool modal, const QString& caption)
    : KDialog(aParent)
{
    setObjectName(QLatin1String("zmodem_progress"));
    setModal(modal);
    setCaption(caption);

    setButtons(KDialog::User1 | KDialog::Close);
    setButtonGuiItem(KDialog::User1, KGuiItem(i18n("&Stop")));
    setDefaultButton(KDialog::Close);
    setEscapeButton(KDialog::User1);
    enableButton(KDialog::Close, false);

    _textEdit = new KTextEdit(this);
    _textEdit->setMinimumSize(400, 100);
    _textEdit->setReadOnly(true);
    setMainWidget(_textEdit);

    connect(this, &Konsole::ZModemDialog::user1Clicked, this, &Konsole::ZModemDialog::slotClose);
    connect(this, &Konsole::ZModemDialog::closeClicked, this, &Konsole::ZModemDialog::slotClose);
}

void ZModemDialog::addProgressText(const QString& text)
{
    QTextCursor currentCursor = _textEdit->textCursor();

    currentCursor.insertBlock();
    currentCursor.insertText(text);
}

void ZModemDialog::transferDone()
{
    enableButton(KDialog::Close, true);
    enableButton(KDialog::User1, false);
}

void ZModemDialog::slotClose()
{
    delayedDestruct();
    accept();
}

