/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2012 Jekyll Wu <adaptee@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
 * A widget for controlling history related options
 */
class HistorySizeWidget : public QWidget
{
    Q_OBJECT

public:
    explicit HistorySizeWidget(QWidget *parent);
    ~HistorySizeWidget() override;

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

    /**
     * Return height which should be set on the widget's label
     * to align with the first widget's item
     */
    int preferredLabelHeight();

Q_SIGNALS:
    /** Emitted when the history mode is changed. */
    void historyModeChanged(Enum::HistoryModeEnum);

    /** Emitted when the history size is changed. */
    void historySizeChanged(int);

private Q_SLOTS:
    void buttonClicked(QAbstractButton *);

private:
    Ui::HistorySizeWidget *_ui;

    // 1000 lines was the default in the KDE3 series
    static const int DefaultLineCount = 1000;
};
}

#endif // HISTORYSIZEWIDGET_H
