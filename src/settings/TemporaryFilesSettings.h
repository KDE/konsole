/*
  SPDX-FileCopyrightText: 2015 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef TEMPORARYFILESSETTINGS_H
#define TEMPORARYFILESSETTINGS_H

#include "ui_TemporaryFilesSettings.h"

namespace Konsole
{
class TemporaryFilesSettings : public QWidget, private Ui::TemporaryFilesSettings
{
    Q_OBJECT

public:
    explicit TemporaryFilesSettings(QWidget *aParent = nullptr);
    ~TemporaryFilesSettings() override = default;
};
}

#endif
