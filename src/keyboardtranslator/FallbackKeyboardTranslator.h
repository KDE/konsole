/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FALLBACKKEYBOARDTRANSLATOR_H
#define FALLBACKKEYBOARDTRANSLATOR_H

// Konsole
#include "KeyboardTranslator.h"

namespace Konsole
{
class KONSOLEPRIVATE_EXPORT FallbackKeyboardTranslator : public KeyboardTranslator
{
public:
    FallbackKeyboardTranslator();
};

}

#endif
