/*
  SPDX-FileCopyrightText: 2012 Jekyll Wu <adaptee@gmail.com>

  SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Own
#include "WindowSystemInfo.h"

#include "config-konsole.h"

#include <QtGlobal>

#if WITH_X11
#include <KWindowSystem>
#include <KX11Extras>
#endif

using Konsole::WindowSystemInfo;

bool WindowSystemInfo::HAVE_TRANSPARENCY = false;

bool WindowSystemInfo::compositingActive()
{
#if WITH_X11
    return !KWindowSystem::isPlatformX11() || KX11Extras::compositingActive();
#else
    return true;
#endif
}
