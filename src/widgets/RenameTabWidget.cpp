/*
    SPDX-FileCopyrightText: 2010 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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

RenameTabWidget::RenameTabWidget(QWidget *parent)
    : QWidget(parent)
    , _ui(nullptr)
{
    _ui = new Ui::RenameTabWidget();
    _ui->setupUi(this);

    _ui->tabTitleEdit->setClearButtonEnabled(true);
    _ui->remoteTabTitleEdit->setClearButtonEnabled(true);

    _colorNone = palette().base().color(); // so that item's text is visible in the combo-box
    _colorNone.setAlpha(0);

    QList<QColor> listColors(_ui->tabColorCombo->colors());
    listColors.insert(0, _colorNone);
    _ui->tabColorCombo->setColors(listColors);
    _ui->tabColorCombo->setItemText(1, i18nc("@label:listbox No color selected", "None"));

    connect(_ui->tabTitleEdit, &QLineEdit::textChanged, this, &Konsole::RenameTabWidget::tabTitleFormatChanged);
    connect(_ui->remoteTabTitleEdit, &QLineEdit::textChanged, this, &Konsole::RenameTabWidget::remoteTabTitleFormatChanged);
    connect(_ui->tabColorCombo, &KColorCombo::activated, this, &Konsole::RenameTabWidget::tabColorChanged);

    _ui->tabTitleFormatButton->setContext(Session::LocalTabTitle);
    connect(_ui->tabTitleFormatButton, &Konsole::TabTitleFormatButton::dynamicElementSelected, this, &Konsole::RenameTabWidget::insertTabTitleText);

    _ui->remoteTabTitleFormatButton->setContext(Session::RemoteTabTitle);
    connect(_ui->remoteTabTitleFormatButton, &Konsole::TabTitleFormatButton::dynamicElementSelected, this, &Konsole::RenameTabWidget::insertRemoteTabTitleText);
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
    if (!color.isValid() || color.alpha() == 0) {
        _ui->tabColorCombo->setColor(_colorNone);
    } else {
        _ui->tabColorCombo->setColor(color);
    }
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
