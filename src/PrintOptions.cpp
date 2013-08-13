/*
  Copyright 2012 Kasper Laudrup <laudrup@stacktrace.dk>

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
  along with this program. If not, see http://www.gnu.org/licenses/.
*/

// Own
#include "PrintOptions.h"

// KDE
#include <KConfigGroup>
#include <KGlobal>

using namespace Konsole;

PrintOptions::PrintOptions(QWidget* aParent) : QWidget(aParent)
{
    setupUi(this);

    KConfigGroup configGroup(KGlobal::config(), "PrintOptions");
    printerFriendly->setChecked(configGroup.readEntry("PrinterFriendly", true));
    scaleOutput->setChecked(configGroup.readEntry("ScaleOutput", true));
}

PrintOptions::~PrintOptions()
{
}

void PrintOptions::saveSettings()
{
    KConfigGroup configGroup(KGlobal::config(), "PrintOptions");
    configGroup.writeEntry("PrinterFriendly", printerFriendly->isChecked());
    configGroup.writeEntry("ScaleOutput", scaleOutput->isChecked());
}
