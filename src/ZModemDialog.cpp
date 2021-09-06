/*
 *  SPDX-FileCopyrightText: 2002 Waldo Bastian <bastian@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-only
 **/

// Own
#include "ZModemDialog.h"

// KDE
#include <KTextEdit>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

using Konsole::ZModemDialog;

ZModemDialog::ZModemDialog(QWidget *aParent, bool modal, const QString &caption)
    : QDialog(aParent)
    , _textEdit(nullptr)
    , mButtonBox(nullptr)
{
    setObjectName(QStringLiteral("zmodem_progress"));
    setModal(modal);
    setWindowTitle(caption);

    mButtonBox = new QDialogButtonBox(QDialogButtonBox::Cancel | QDialogButtonBox::Close);
    auto mainWidget = new QWidget(this);
    auto mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    mainLayout->addWidget(mButtonBox);

    // Use Cancel here to stop the transfer
    mButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(true);
    mButtonBox->button(QDialogButtonBox::Close)->setEnabled(false);

    connect(mButtonBox, &QDialogButtonBox::rejected, this, &Konsole::ZModemDialog::slotCancel);
    connect(mButtonBox, &QDialogButtonBox::accepted, this, &Konsole::ZModemDialog::slotClose);

    _textEdit = new KTextEdit(this);
    _textEdit->setMinimumSize(400, 100);
    _textEdit->setReadOnly(true);
    mainLayout->addWidget(_textEdit);

    addText(QStringLiteral("Note: pressing Cancel will almost certainly cause the terminal to be unusable."));
    addText(QStringLiteral("-----------------"));
}

void ZModemDialog::addText(const QString &text)
{
    _textEdit->append(text);
}

void ZModemDialog::addProgressText(const QString &text)
{
    _textEdit->insertPlainText(text);
}

void ZModemDialog::slotCancel()
{
    Q_EMIT zmodemCancel();
    slotClose();
}

void ZModemDialog::transferDone()
{
    mButtonBox->button(QDialogButtonBox::Cancel)->setEnabled(false);
    mButtonBox->button(QDialogButtonBox::Close)->setEnabled(true);
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
