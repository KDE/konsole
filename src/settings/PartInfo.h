/*
  SPDX-FileCopyrightText: 2017 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef PARTINFO_H
#define PARTINFO_H

#include "ui_PartInfo.h"

namespace Konsole
{
class PartInfoSettings : public QWidget, private Ui::PartInfoSettings
{
    Q_OBJECT

public:
    explicit PartInfoSettings(QWidget *parent = nullptr);
    ~PartInfoSettings() override;
};
}

#endif
