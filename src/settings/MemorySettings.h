/*
  SPDX-FileCopyrightText: 2023 Theodore Wang <theodorewang12@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef MEMORYSETTINGS_H
#define MEMORYSETTINGS_H

#include "ui_MemorySettings.h"

namespace Konsole
{
class MemorySettings : public QWidget, private Ui::MemorySettings
{
    Q_OBJECT

public:
    explicit MemorySettings(QWidget *parent = nullptr);
    ~MemorySettings() override;
};
}

#endif
