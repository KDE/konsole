/*
    SPDX-FileCopyrightText: 2010 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "RenameTabDialog.h"

// Konsole
#include "Shortcut_p.h"
#include "ui_RenameTabDialog.h"

#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

using Konsole::RenameTabDialog;

RenameTabDialog::RenameTabDialog(QWidget *parent)
    : QDialog(parent)
    , _ui(nullptr)
{
    setWindowTitle(i18n("Tab Properties"));
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto mainWidget = new QWidget(this);
    auto mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setAutoDefault(true);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &RenameTabDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &RenameTabDialog::reject);
    mainLayout->addWidget(buttonBox);

    setWindowModality(Qt::WindowModal);

    _ui = new Ui::RenameTabDialog();
    _ui->setupUi(mainWidget);
}

RenameTabDialog::~RenameTabDialog()
{
    delete _ui;
}

void RenameTabDialog::focusTabTitleText()
{
    _ui->renameTabWidget->focusTabTitleText();
}

void RenameTabDialog::focusRemoteTabTitleText()
{
    _ui->renameTabWidget->focusRemoteTabTitleText();
}

void RenameTabDialog::setTabTitleText(const QString &text)
{
    _ui->renameTabWidget->setTabTitleText(text);
}

void RenameTabDialog::setRemoteTabTitleText(const QString &text)
{
    _ui->renameTabWidget->setRemoteTabTitleText(text);
}

void RenameTabDialog::setColor(const QColor &color)
{
    _ui->renameTabWidget->setColor(color);
}

QString RenameTabDialog::tabTitleText() const
{
    return _ui->renameTabWidget->tabTitleText();
}

QString RenameTabDialog::remoteTabTitleText() const
{
    return _ui->renameTabWidget->remoteTabTitleText();
}

QColor RenameTabDialog::color() const
{
    return _ui->renameTabWidget->color();
}
