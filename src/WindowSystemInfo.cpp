/*
  SPDX-FileCopyrightText: 2012 Jekyll Wu <adaptee@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Own
#include "WindowSystemInfo.h"

#include "config-konsole.h"

#include <kwindowsystem_version.h>

#include <QtGlobal>

#include <KWindowSystem>

#if HAVE_X11
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(5, 101, 0)
#include <KX11Extras>
#endif
#endif

using Konsole::WindowSystemInfo;

bool WindowSystemInfo::HAVE_TRANSPARENCY = false;

bool WindowSystemInfo::compositingActive()
{
#if KWINDOWSYSTEM_VERSION >= QT_VERSION_CHECK(5, 101, 0)
#if HAVE_X11
    return KX11Extras::compositingActive();
#else
    return true;
#endif
#else
    return KWindowSystem::compositingActive();
#endif
}
