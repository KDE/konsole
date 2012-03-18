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

    _ui->tabTitleEdit->setClearButtonShown(true);
    _ui->remoteTabTitleEdit->setClearButtonShown(true);

    _ui->tabTitleFormatButton->setContext(Session::LocalTabTitle);
    connect(_ui->tabTitleFormatButton, SIGNAL(dynamicElementSelected(QString)),
            this, SLOT(insertTabTitleText(QString)));

    _ui->remoteTabTitleFormatButton->setContext(Session::RemoteTabTitle);
    connect(_ui->remoteTabTitleFormatButton, SIGNAL(dynamicElementSelected(QString)),
            this, SLOT(insertRemoteTabTitleText(QString)));
}

RenameTabsDialog::~RenameTabsDialog()
{
    delete _ui;
}

void RenameTabsDialog::focusTabTitleText()
{
    _ui->tabTitleEdit->setFocus();
}

void RenameTabsDialog::focusRemoteTabTitleText()
{
    _ui->remoteTabTitleEdit->setFocus();
}

void RenameTabsDialog::setTabTitleText(const QString& text)
{
    _ui->tabTitleEdit->setText(text);
}

void RenameTabsDialog::setRemoteTabTitleText(const QString& text)
{
    _ui->remoteTabTitleEdit->setText(text);
}

QString RenameTabsDialog::tabTitleText() const
{
    return(_ui->tabTitleEdit->text());
}

QString RenameTabsDialog::remoteTabTitleText() const
{
    return(_ui->remoteTabTitleEdit->text());
}

void RenameTabsDialog::insertTabTitleText(const QString& text)
{
    _ui->tabTitleEdit->insert(text);
}

void RenameTabsDialog::insertRemoteTabTitleText(const QString& text)
{
    _ui->remoteTabTitleEdit->insert(text);
}

