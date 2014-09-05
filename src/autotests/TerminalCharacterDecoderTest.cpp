/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2013 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "TerminalCharacterDecoderTest.h"

// Qt
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

#include <QFile>
#include <QXmlSimpleReader>

// KDE
#include <qtest_kde.h>

using namespace Konsole;

void TerminalCharacterDecoderTest::init()
{
}

void TerminalCharacterDecoderTest::cleanup()
{
}

void TerminalCharacterDecoderTest::testPlainTextDecoder()
{
    TerminalCharacterDecoder* decoder = new PlainTextDecoder();
    Character characters[6];
    characters[0] = Character('h');
    characters[1] = Character('e');
    characters[2] = Character('l');
    characters[3] = Character('l');
    characters[4] = Character('o');
    characters[5] = Character(' ');
    characters[5].isRealCharacter = false;
    QString outputString;
    QTextStream outputStream(&outputString);
    decoder->begin(&outputStream);
    decoder->decodeLine(characters, 6, LINE_DEFAULT);
    decoder->end();
    QCOMPARE(outputString, QString("hello"));
    delete decoder;
}

void TerminalCharacterDecoderTest::testHTMLFileForValidity()
{
    QString fileName = "konsole.html";
    QFile fi(fileName);

    if (!fi.exists())
        QSKIP("Test html file not found.");

    QXmlSimpleReader xmlReader;
    QXmlInputSource source(&fi);

    QVERIFY(xmlReader.parse(&source));
}

QTEST_KDEMAIN_CORE(TerminalCharacterDecoderTest)

