/*
  SPDX-FileCopyrightText: 2012 Kasper Laudrup <laudrup@stacktrace.dk>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef PRINTOPTIONS_H
#define PRINTOPTIONS_H

#include "ui_PrintOptions.h"

namespace Konsole
{
class PrintOptions : public QWidget, private Ui::PrintOptions
{
    Q_OBJECT

public:
    explicit PrintOptions(QWidget *parent = nullptr);
    ~PrintOptions() override;

public Q_SLOTS:
    void saveSettings();
};
}

#endif
