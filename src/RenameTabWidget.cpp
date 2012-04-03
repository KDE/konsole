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
#include "RenameTabWidget.h"

// Konsole
#include "ui_RenameTabWidget.h"

using Konsole::RenameTabWidget;

RenameTabWidget::RenameTabWidget(QWidget* parent)
    : QWidget(parent)
{
    _ui = new Ui::RenameTabWidget();
    _ui->setupUi(this);

    _ui->tabTitleEdit->setClearButtonShown(true);
    _ui->remoteTabTitleEdit->setClearButtonShown(true);

    connect(_ui->tabTitleEdit, SIGNAL(textChanged(QString)), this,
            SIGNAL(tabTitleFormatChanged(QString)));
    connect(_ui->remoteTabTitleEdit, SIGNAL(textChanged(QString)), this,
            SIGNAL(remoteTabTitleFormatChanged(QString)));

    _ui->tabTitleFormatButton->setContext(Session::LocalTabTitle);
    connect(_ui->tabTitleFormatButton, SIGNAL(dynamicElementSelected(QString)),
            this, SLOT(insertTabTitleText(QString)));

    _ui->remoteTabTitleFormatButton->setContext(Session::RemoteTabTitle);
    connect(_ui->remoteTabTitleFormatButton, SIGNAL(dynamicElementSelected(QString)),
            this, SLOT(insertRemoteTabTitleText(QString)));
}

RenameTabWidget::~RenameTabWidget()
{
    delete _ui;
}

void RenameTabWidget::focusTabTitleText()
{
    _ui->tabTitleEdit->setFocus();
}

void RenameTabWidget::focusRemoteTabTitleText()
{
    _ui->remoteTabTitleEdit->setFocus();
}

void RenameTabWidget::setTabTitleText(const QString& text)
{
    _ui->tabTitleEdit->setText(text);
}

void RenameTabWidget::setRemoteTabTitleText(const QString& text)
{
    _ui->remoteTabTitleEdit->setText(text);
}

QString RenameTabWidget::tabTitleText() const
{
    return(_ui->tabTitleEdit->text());
}

QString RenameTabWidget::remoteTabTitleText() const
{
    return(_ui->remoteTabTitleEdit->text());
}

void RenameTabWidget::insertTabTitleText(const QString& text)
{
    _ui->tabTitleEdit->insert(text);
    focusTabTitleText();
}

void RenameTabWidget::insertRemoteTabTitleText(const QString& text)
{
    _ui->remoteTabTitleEdit->insert(text);
    focusRemoteTabTitleText();
}

