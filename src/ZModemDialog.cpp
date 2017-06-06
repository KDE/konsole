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
#include <QDialogButtonBox>
#include <QPushButton>
#include <KGuiItem>
#include <QVBoxLayout>

using Konsole::ZModemDialog;

ZModemDialog::ZModemDialog(QWidget *aParent, bool modal, const QString &caption) :
    QDialog(aParent)
{
    setObjectName(QStringLiteral("zmodem_progress"));
    setModal(modal);
    setWindowTitle(caption);

    mButtonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    auto mainWidget = new QWidget(this);
    auto mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    mUser1Button = new QPushButton;
    mButtonBox->addButton(mUser1Button, QDialogButtonBox::ActionRole);
    mainLayout->addWidget(mButtonBox);
    KGuiItem::assign(mUser1Button, KGuiItem(i18n("&Stop")));
    mButtonBox->button(QDialogButtonBox::Close)->setDefault(true);
    mUser1Button->setShortcut(Qt::Key_Escape);
    mButtonBox->button(QDialogButtonBox::Close)->setEnabled(false);

    _textEdit = new KTextEdit(this);
    _textEdit->setMinimumSize(400, 100);
    _textEdit->setReadOnly(true);
    mainLayout->addWidget(_textEdit);

    connect(this, &Konsole::ZModemDialog::user1Clicked, this,
            &Konsole::ZModemDialog::slotUser1Clicked);
    connect(mButtonBox->button(QDialogButtonBox::Close),
            &QPushButton::clicked, this,
            &Konsole::ZModemDialog::slotClose);
}

void ZModemDialog::addProgressText(const QString &text)
{
    QTextCursor currentCursor = _textEdit->textCursor();

    currentCursor.insertBlock();
    currentCursor.insertText(text);
}

void ZModemDialog::slotUser1Clicked()
{
    Q_EMIT user1Clicked();
    slotClose();
}

void ZModemDialog::transferDone()
{
    mButtonBox->button(QDialogButtonBox::Close)->setEnabled(true);
    mUser1Button->setEnabled(false);
}

void ZModemDialog::slotClose()
{
    delayedDestruct();
    accept();
}

void ZModemDialog::delayedDestruct()
{
    if (isVisible()) {
        hide();
    }

    deleteLater();
}
