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
#include "widgets/RenameTabWidget.h"

// Konsole
#include "ui_RenameTabWidget.h"

// KDE
#include <KColorCombo>

// Qt
#include <QColor>

using Konsole::RenameTabWidget;

RenameTabWidget::RenameTabWidget(QWidget *parent) :
    QWidget(parent),
    _ui(nullptr)
{
    _ui = new Ui::RenameTabWidget();
    _ui->setupUi(this);

    _ui->tabTitleEdit->setClearButtonEnabled(true);
    _ui->remoteTabTitleEdit->setClearButtonEnabled(true);

    QList<QColor> listColors(_ui->tabColorCombo->colors());
    listColors.insert(0, QColor(QColor::Invalid));
    _ui->tabColorCombo->setColors(listColors);
    _ui->tabColorCombo->setItemText(1, i18n("Color from theme"));

    connect(_ui->tabTitleEdit, &QLineEdit::textChanged, this,
            &Konsole::RenameTabWidget::tabTitleFormatChanged);
    connect(_ui->remoteTabTitleEdit, &QLineEdit::textChanged, this,
            &Konsole::RenameTabWidget::remoteTabTitleFormatChanged);
    connect(_ui->tabColorCombo, &KColorCombo::activated, this,
            &Konsole::RenameTabWidget::tabColorChanged);

    _ui->tabTitleFormatButton->setContext(Session::LocalTabTitle);
    connect(_ui->tabTitleFormatButton, &Konsole::TabTitleFormatButton::dynamicElementSelected, this,
            &Konsole::RenameTabWidget::insertTabTitleText);

    _ui->remoteTabTitleFormatButton->setContext(Session::RemoteTabTitle);
    connect(_ui->remoteTabTitleFormatButton, &Konsole::TabTitleFormatButton::dynamicElementSelected,
            this, &Konsole::RenameTabWidget::insertRemoteTabTitleText);
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

void RenameTabWidget::setTabTitleText(const QString &text)
{
    _ui->tabTitleEdit->setText(text);
}

void RenameTabWidget::setRemoteTabTitleText(const QString &text)
{
    _ui->remoteTabTitleEdit->setText(text);
}

void RenameTabWidget::setColor(const QColor &color)
{
    _ui->tabColorCombo->setColor(color);
}

QString RenameTabWidget::tabTitleText() const
{
    return _ui->tabTitleEdit->text();
}

QString RenameTabWidget::remoteTabTitleText() const
{
    return _ui->remoteTabTitleEdit->text();
}

QColor RenameTabWidget::color() const
{
    return _ui->tabColorCombo->color();
}

void RenameTabWidget::insertTabTitleText(const QString &text)
{
    _ui->tabTitleEdit->insert(text);
    focusTabTitleText();
}

void RenameTabWidget::insertRemoteTabTitleText(const QString &text)
{
    _ui->remoteTabTitleEdit->insert(text);
    focusRemoteTabTitleText();
}
