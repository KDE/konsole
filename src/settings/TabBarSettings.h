/*
  SPDX-FileCopyrightText: 2011 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef TABBARSETTINGS_H
#define TABBARSETTINGS_H

#include "ui_TabBarSettings.h"

namespace Konsole
{
class TabBarSettings : public QWidget, private Ui::TabBarSettings
{
    Q_OBJECT

public:
    explicit TabBarSettings(QWidget *parent = nullptr);
    ~TabBarSettings() override;
};
}

#endif
