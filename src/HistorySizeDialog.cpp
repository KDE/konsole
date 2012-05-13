/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2012 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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

// Konsole
#include "ui_HistorySizeDialog.h"

using namespace Konsole;

HistorySizeDialog::HistorySizeDialog(QWidget* parent)
    : KDialog(parent)
{
    setCaption(i18nc("@title:window", "Adjust Scrollback"));
    setButtons(KDialog::Ok | KDialog::Cancel);

    setWindowModality(Qt::WindowModal);

    _ui = new Ui::HistorySizeDialog();
    _ui->setupUi(mainWidget());

    _ui->tempWarningWidget->setVisible(true);
    _ui->tempWarningWidget->setWordWrap(true);
    _ui->tempWarningWidget->setCloseButtonVisible(false);
    _ui->tempWarningWidget->setMessageType(KMessageWidget::Information);
    _ui->tempWarningWidget->setText(i18nc("@info:status",
        "Any adjustments are only temporary to this session."));
}

HistorySizeDialog::~HistorySizeDialog()
{
    delete _ui;
}

void HistorySizeDialog::setMode(Enum::HistoryModeEnum aMode)
{
    _ui->historySizeWidget->setMode(aMode);
}

Enum::HistoryModeEnum HistorySizeDialog::mode() const
{
    return _ui->historySizeWidget->mode();
}

int HistorySizeDialog::lineCount() const
{
    return _ui->historySizeWidget->lineCount();
}

void HistorySizeDialog::setLineCount(int lines)
{
    _ui->historySizeWidget->setLineCount(lines);
}

#include "HistorySizeDialog.moc"
