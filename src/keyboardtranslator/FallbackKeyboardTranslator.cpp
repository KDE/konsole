/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "FallbackKeyboardTranslator.h"

using namespace Konsole;

FallbackKeyboardTranslator::FallbackKeyboardTranslator()
    : KeyboardTranslator(QStringLiteral("fallback"))
{
    setDescription(QStringLiteral("Fallback Keyboard Translator"));

    // Key "TAB" should send out '\t'
    KeyboardTranslator::Entry entry;
    entry.setKeyCode(Qt::Key_Tab);
    entry.setText("\t");
    addEntry(entry);
}
