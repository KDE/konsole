/*
  SPDX-FileCopyrightText: 2015 Kurt Hindenburg <kurt.hindenburg@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Own
#include "TemporaryFilesSettings.h"

// Qt
#include <QStandardPaths>

using namespace Konsole;

TemporaryFilesSettings::TemporaryFilesSettings(QWidget *aParent)
    : QWidget(aParent)
{
    setupUi(this);

    const QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
#ifdef Q_OS_UNIX
    // Use "~" instead of full path. It looks nice and helps
    // in cases when home path is really long.
    const QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    if (cachePath.startsWith(homePath)) {
        cachePath.replace(0, homePath.length(), QStringLiteral("~"));
    }
#endif

    // There's no way of doing this with strings placed in .ui file
    kcfg_scrollbackUseSystemLocation->setText(
        i18nc("@option:radio File location; <filename>%1</filename>: path to directory placeholder", "System temporary directory (%1)", tempPath));
    kcfg_scrollbackUseCacheLocation->setText(
        i18nc("@option:radio File location; <filename>%1</filename>: path to directory placeholder", "User cache directory (%1)", cachePath));

    kcfg_scrollbackUseSpecifiedLocationDirectory->setMode(KFile::Directory);
}
