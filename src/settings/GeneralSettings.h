/*
  SPDX-FileCopyrightText: 2011 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef GENERALSETTINGS_H
#define GENERALSETTINGS_H

#include "ui_GeneralSettings.h"

namespace Konsole
{
class GeneralSettings : public QWidget, private Ui::GeneralSettings
{
    Q_OBJECT

public:
    explicit GeneralSettings(QWidget *aParent = nullptr);
    ~GeneralSettings() override;

public Q_SLOTS:
    void slotEnableAllMessages();
};
}

#endif
