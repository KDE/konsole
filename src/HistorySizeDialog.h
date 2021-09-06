/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2012 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HISTORYSIZEDIALOG_H
#define HISTORYSIZEDIALOG_H

// KDE
#include <QDialog>

// Konsole
#include "Enumeration.h"

namespace Ui
{
class HistorySizeDialog;
}

namespace Konsole
{
class HistorySizeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistorySizeDialog(QWidget *parent = nullptr);
    ~HistorySizeDialog() override;

    /** See HistorySizeWidget::setMode. */
    void setMode(Enum::HistoryModeEnum aMode);

    /** See HistorySizeWidget::mode. */
    Enum::HistoryModeEnum mode() const;

    /** See HistorySizeWidget::setLineCount. */
    void setLineCount(int lines);

    /** See HistorySizeWidget::lineCount. */
    int lineCount() const;

    QSize sizeHint() const override;

private:
    Ui::HistorySizeDialog *_ui;
};
}

#endif // HISTORYSIZEDIALOG_H
