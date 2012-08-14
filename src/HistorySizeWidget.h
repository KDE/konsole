/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2012 by Jekyll Wu <adaptee@gmail.com>

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

#ifndef HISTORYSIZEWIDGET_H
#define HISTORYSIZEWIDGET_H

// Qt
#include <QWidget>

// Konsole
#include "Enumeration.h"

class QAbstractButton;

namespace Ui
{
class HistorySizeWidget;
}

namespace Konsole
{
/**
 * A widget for controlling hisotry related options
 */
class HistorySizeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistorySizeWidget(QWidget* parent);
    virtual ~HistorySizeWidget();

    /** Specifies the history mode. */
    void setMode(Enum::HistoryModeEnum aMode);

    /** Returns the history mode chosen by the user. */
    Enum::HistoryModeEnum mode() const;

    /** Sets the number of lines for the fixed size history mode. */
    void setLineCount(int lines);

    /**
     * Returns the number of lines of history to remember.
     * This is only valid when mode() == FixedSizeHistory,
     * and returns 0 otherwise.
     */
    int lineCount() const;

signals:
    /** Emitted when the history mode is changed. */
    void historyModeChanged(Enum::HistoryModeEnum) const;

    /** Emitted when the history size is changed. */
    void historySizeChanged(int) const;

private slots:
    void buttonClicked(QAbstractButton*) const;

private:
    Ui::HistorySizeWidget* _ui;

    // 1000 lines was the default in the KDE3 series
    static const int DefaultLineCount = 1000;
};
}

#endif // HISTORYSIZEWIDGET_H
