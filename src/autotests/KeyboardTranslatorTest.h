/*
    SPDX-FileCopyrightText: 2013 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KEYBOARDTRANSLATORTEST_H
#define KEYBOARDTRANSLATORTEST_H

#include "keyboardtranslator/KeyboardTranslator.h"
#include <QObject>

namespace Konsole
{
class KeyboardTranslatorTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testEntryTextWildcards();
    void testEntryTextWildcards_data();
    void testFallback();
    void testHexKeys();
};

}

#endif // KEYBOARDTRANSLATORTEST_H
