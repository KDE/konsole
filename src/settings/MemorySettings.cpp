/*
  SPDX-FileCopyrightText: 2023 Theodore Wang <theodorewang12@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Own
#include "MemorySettings.h"

// Qt
#include <QCheckBox>
#include <QLabel>

using namespace Konsole;

MemorySettings::MemorySettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);
}

MemorySettings::~MemorySettings() = default;

#include "moc_MemorySettings.cpp"
