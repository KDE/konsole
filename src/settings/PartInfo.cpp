/*
  SPDX-FileCopyrightText: 2017 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Own
#include "PartInfo.h"

using namespace Konsole;

PartInfoSettings::PartInfoSettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

PartInfoSettings::~PartInfoSettings() = default;
