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

#ifndef HISTORYSIZEDIALOG_H
#define HISTORYSIZEDIALOG_H

// KDE
#include <KDialog>

// Konsole
#include "Enumeration.h"

namespace Konsole
{

class HistorySizeWidget;

/**
 * A dialog for controlling history related options
 * It is only a simple container of HistorySizeWidget
 */
class HistorySizeDialog : public KDialog
{
    Q_OBJECT

public:
    explicit HistorySizeDialog(QWidget* parent);

    /** See HistorySizeWidget::setMode. */
    void setMode(Enum::HistoryModeEnum aMode);

    /** See HistorySizeWidget::mode. */
    Enum::HistoryModeEnum mode() const;

    /** See HistorySizeWidget::setLineCount. */
    void setLineCount(int lines);

    /** See HistorySizeWidget::lineCount. */
    int lineCount() const;

signals:
    /**
     * Emitted when the user changes the scroll-back mode or line count and
     * accepts the change by pressing the OK button
     *
     * @param mode The current history mode.  This is a value from the HistoryMode enum.
     * @param lineCount The current line count.  This is only applicable if mode is
     * FixedSizeHistory
     */
    void optionsChanged(int mode , int lineCount);

private slots:

    // fires the optionsChanged() signal with the current mode
    // and line count as arguments
    void emitOptionsChanged();

private:
    HistorySizeWidget* _historySizeWidget;

};

}

#endif // HISTORYSIZEDIALOG_H
