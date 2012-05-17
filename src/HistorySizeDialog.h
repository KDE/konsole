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

#ifndef HISTORYSIZEDIALOG_H
#define HISTORYSIZEDIALOG_H

// KDE
#include <KDialog>

// Konsole
#include "Enumeration.h"

namespace Ui
{
class HistorySizeDialog;
}

namespace Konsole
{
class HistorySizeDialog : public KDialog
{
    Q_OBJECT

public:
    explicit HistorySizeDialog(QWidget* parent = 0);
    ~HistorySizeDialog();

    /** See HistorySizeWidget::setMode. */
    void setMode(Enum::HistoryModeEnum aMode);

    /** See HistorySizeWidget::mode. */
    Enum::HistoryModeEnum mode() const;

    /** See HistorySizeWidget::setLineCount. */
    void setLineCount(int lines);

    /** See HistorySizeWidget::lineCount. */
    int lineCount() const;

private:
    Ui::HistorySizeDialog* _ui;
};
}

#endif // HISTORYSIZEDIALOG_H
