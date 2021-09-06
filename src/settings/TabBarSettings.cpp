/*
  SPDX-FileCopyrightText: 2011 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Own
#include "TabBarSettings.h"

using namespace Konsole;

TabBarSettings::TabBarSettings(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    // For some reason these layouts have invalid sizes when
    // sizeHint() is read before the widget is shown.
    appearanceTabLayout->activate();
    behaviorTabLayout->activate();

    // Enable CSS file selector only when tabbar is visible and custom css is active
    const auto updateStyleSheetFileEnable = [this](bool) {
        kcfg_TabBarUserStyleSheetFile->setEnabled(kcfg_TabBarUseUserStyleSheet->isChecked() && !AlwaysHideTabBar->isChecked());
    };
    connect(kcfg_TabBarUseUserStyleSheet, &QAbstractButton::toggled, this, updateStyleSheetFileEnable);
    connect(AlwaysHideTabBar, &QAbstractButton::toggled, this, updateStyleSheetFileEnable);
}

TabBarSettings::~TabBarSettings() = default;
