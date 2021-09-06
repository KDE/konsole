/*
  SPDX-FileCopyrightText: 2012 Kasper Laudrup <laudrup@stacktrace.dk>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Own
#include "PrintOptions.h"

// KDE
#include <KConfigGroup>
#include <KSharedConfig>

using namespace Konsole;

PrintOptions::PrintOptions(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    KConfigGroup configGroup(KSharedConfig::openConfig(), "PrintOptions");
    printerFriendly->setChecked(configGroup.readEntry("PrinterFriendly", true));
    scaleOutput->setChecked(configGroup.readEntry("ScaleOutput", true));
}

PrintOptions::~PrintOptions() = default;

void PrintOptions::saveSettings()
{
    KConfigGroup configGroup(KSharedConfig::openConfig(), "PrintOptions");
    configGroup.writeEntry("PrinterFriendly", printerFriendly->isChecked());
    configGroup.writeEntry("ScaleOutput", scaleOutput->isChecked());
}
