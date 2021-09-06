/*
  SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@kde.org>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "ThumbnailsSettings.h"

using namespace Konsole;

ThumbnailsSettings::ThumbnailsSettings(QWidget *aParent)
    : QWidget(aParent)
{
    setupUi(this);
}
