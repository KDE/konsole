/*
  Copyright 2011 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of
  the License or (at your option) version 3 or any later version
  accepted by the membership of KDE e.V. (or its successor appro-
  ved by the membership of KDE e.V.), which shall act as a proxy
  defined in Section 14 of version 3 of the license.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program. If not, see https://www.gnu.org/licenses/.
*/

// Own
#include "TabBarSettings.h"

using namespace Konsole;

TabBarSettings::TabBarSettings(QWidget* parent) : QWidget(parent)
{
    setupUi(this);

    // For some reason these layouts have invalid sizes when
    // sizeHint() is read before the widget is shown.
    appearanceTabLayout->activate();
    behaviorTabLayout->activate();

    // Enable CSS file selector only when tabbar is visible and custom css is active
    const auto updateStyleSheetFileEnable = [this](bool) {
        kcfg_TabBarUserStyleSheetFile->setEnabled(kcfg_TabBarUseUserStyleSheet->isChecked()
                                                  && !AlwaysHideTabBar->isChecked());
    };
    connect(kcfg_TabBarUseUserStyleSheet, &QAbstractButton::toggled,
            this, updateStyleSheetFileEnable);
    connect(AlwaysHideTabBar, &QAbstractButton::toggled,
            this, updateStyleSheetFileEnable);
}

TabBarSettings::~TabBarSettings() = default;

