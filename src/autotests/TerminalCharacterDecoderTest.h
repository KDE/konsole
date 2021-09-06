/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2018 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALCHARACTERDECODERTEST_H
#define TERMINALCHARACTERDECODERTEST_H

#include <QObject>

#include "../characters/Character.h"

namespace Konsole
{
class TerminalCharacterDecoderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    Character *convertToCharacter(const QString &text, QVector<RenditionFlags> renditions);
    void testPlainTextDecoder();
    void testPlainTextDecoder_data();
    void testHTMLDecoder();
    void testHTMLDecoder_data();
};

}

#endif // TERMINALCHARACTERDECODERTEST_H
