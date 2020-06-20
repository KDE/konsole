/*
  Copyright 2015 Kurt Hindenburg <kurt.hindenburg@gmail.com>

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
#include "TemporaryFilesSettings.h"

// Qt
#include <QStandardPaths>

using namespace Konsole;



TemporaryFilesSettings::TemporaryFilesSettings(QWidget* aParent) : QWidget(aParent)
{
    setupUi(this);

    const QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#ifdef Q_OS_UNIX
    // Use "~" instead of full path. It looks nice and helps
    // in cases when home path is really long.
    const QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if(cachePath.startsWith(homePath)) {
        cachePath.replace(0, homePath.length(), QStringLiteral("~"));
    }
#endif

    // There's no way of doing this with strings placed in .ui file
    kcfg_scrollbackUseSystemLocation->setText(
            i18nc("@option:radio File location; <filename>%1</filename>: path to directory placeholder",
                  "System temporary directory (%1)", tempPath));
    kcfg_scrollbackUseCacheLocation->setText(
            i18nc("@option:radio File location; <filename>%1</filename>: path to directory placeholder",
                  "User cache directory (%1)", cachePath));

    kcfg_scrollbackUseSpecifiedLocationDirectory->setMode(KFile::Directory);
}
