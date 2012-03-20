/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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
#include "HistorySizeDialog.h"

// Qt
#include <QtGui/QVBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QWidget>

// KDE
#include <KLocalizedString>

// Konsole
#include "HistorySizeWidget.h"

using namespace Konsole;

HistorySizeDialog::HistorySizeDialog(QWidget* aParent)
    :  KDialog(aParent)
    , _historySizeWidget(0)
{
    // basic dialog properties
    setPlainCaption(i18n("Adjust Scrollback"));
    setButtons(KDialog::Ok | KDialog::Cancel);
    setDefaultButton(KDialog::Ok);
    setModal(false);

    // dialog widgets
    QWidget* dialogWidget = new QWidget(this);
    setMainWidget(dialogWidget);

    QVBoxLayout* dialogLayout = new QVBoxLayout(dialogWidget);

    QLabel* warningLabel = new QLabel(i18n("<center>The adjustment is only temporary</center>"), this);
    warningLabel->setStyleSheet("text-align:center; font-weight:normal; color:palette(dark)");

    _historySizeWidget = new HistorySizeWidget(this);

    dialogLayout->addWidget(warningLabel);
    dialogLayout->insertSpacing(-1, 5);
    dialogLayout->addWidget(_historySizeWidget);
    dialogLayout->insertSpacing(-1, 10);

    connect(this, SIGNAL(accepted()), this, SLOT(emitOptionsChanged()));
}

void HistorySizeDialog::emitOptionsChanged()
{
    emit optionsChanged(mode() , lineCount());
}

void HistorySizeDialog::setMode(Enum::HistoryModeEnum aMode)
{
    _historySizeWidget->setMode(aMode);
}

Enum::HistoryModeEnum HistorySizeDialog::mode() const
{
    return _historySizeWidget->mode();
}

int HistorySizeDialog::lineCount() const
{
    return _historySizeWidget->lineCount();
}

void HistorySizeDialog::setLineCount(int lines)
{
    _historySizeWidget->setLineCount(lines);
}

#include "HistorySizeDialog.moc"
