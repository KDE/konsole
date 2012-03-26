/*
    Copyright 2010 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "RenameTabsDialog.h"

// Konsole
#include "ui_RenameTabsDialog.h"

using Konsole::RenameTabsDialog;

RenameTabsDialog::RenameTabsDialog(QWidget* parent)
    : KDialog(parent)
{
    setCaption(i18n("Rename Tab"));
    setButtons(KDialog::Ok | KDialog::Cancel);

    setWindowModality(Qt::WindowModal);

    _ui = new Ui::RenameTabsDialog();
    _ui->setupUi(mainWidget());
}

RenameTabsDialog::~RenameTabsDialog()
{
    delete _ui;
}

void RenameTabsDialog::focusTabTitleText()
{
    _ui->renameTabWidget->focusTabTitleText();
}

void RenameTabsDialog::focusRemoteTabTitleText()
{
    _ui->renameTabWidget->focusRemoteTabTitleText();
}

void RenameTabsDialog::setTabTitleText(const QString& text)
{
    _ui->renameTabWidget->setTabTitleText(text);
}

void RenameTabsDialog::setRemoteTabTitleText(const QString& text)
{
    _ui->renameTabWidget->setRemoteTabTitleText(text);
}

QString RenameTabsDialog::tabTitleText() const
{
    return _ui->renameTabWidget->tabTitleText();
}

QString RenameTabsDialog::remoteTabTitleText() const
{
    return _ui->renameTabWidget->remoteTabTitleText();
}

void RenameTabsDialog::insertTabTitleText(const QString& text)
{
    _ui->renameTabWidget->insertTabTitleText(text);
}

void RenameTabsDialog::insertRemoteTabTitleText(const QString& text)
{
    _ui->renameTabWidget->insertRemoteTabTitleText(text);
}

