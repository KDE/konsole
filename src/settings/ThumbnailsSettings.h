/*
  SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@kde.org>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef THUMBNAILSSETTINGS_H
#define THUMBNAILSSETTINGS_H

#include "ui_ThumbnailsSettings.h"

namespace Konsole
{
class ThumbnailsSettings : public QWidget, private Ui::ThumbnailsSettings
{
    Q_OBJECT

public:
    explicit ThumbnailsSettings(QWidget *aParent = nullptr);
    ~ThumbnailsSettings() override = default;
};

}

#endif
