/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2013, 2018 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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
#include <QTextStream>


// KDE
#include <qtest.h>

using namespace Konsole;

Character* TerminalCharacterDecoderTest::convertToCharacter(const QString &text, QVector<RenditionFlags> renditions)
{
    auto charResult = new Character[text.size()];

    // Force renditions size to match that of text; default will be DEFAULT_RENDITION.
    if (renditions.size() < text.size()) {
        renditions.resize(text.size());
    }
    for (int i = 0; i < text.size(); ++i) {
        charResult[i] = Character(text.at(i).unicode());
        charResult[i].rendition = renditions.at(i);
    }
    return charResult;
}

void TerminalCharacterDecoderTest::testPlainTextDecoder_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QVector<RenditionFlags>>("renditions");
    QTest::addColumn<QString>("result");

    /* Notes:
     * - rendition has no effect on plain decoded text
     *
     * TODO: need to add foregroundColor, backgroundColor, and isRealCharacter
     */
    QTest::newRow("simple text with default rendition") << "hello" << QVector<RenditionFlags>(6).fill(DEFAULT_RENDITION) << "hello";
    QTest::newRow("simple text with bold rendition") << "hello" << QVector<RenditionFlags>(6).fill(RE_BOLD) << "hello";
    QTest::newRow("simple text with underline and italic rendition") << "hello" << QVector<RenditionFlags>(6).fill(RE_UNDERLINE|RE_ITALIC) << "hello";

    QTest::newRow("simple text with default rendition (shorten)") << "hello" << QVector<RenditionFlags>(4).fill(DEFAULT_RENDITION) << "hello";
    QTest::newRow("simple text with underline rendition (shorten)") << "hello" << QVector<RenditionFlags>(4).fill(RE_UNDERLINE) << "hello";
}

void TerminalCharacterDecoderTest::testPlainTextDecoder()
{
    QFETCH(QString, text);
    QFETCH(QVector<RenditionFlags>, renditions);
    QFETCH(QString, result);

    TerminalCharacterDecoder *decoder = new PlainTextDecoder();
    auto testCharacters = convertToCharacter(text, renditions);
    QString outputString;
    QTextStream outputStream(&outputString);
    decoder->begin(&outputStream);
    decoder->decodeLine(testCharacters, text.size(), /* ignored */ LINE_DEFAULT);
    decoder->end();
    QCOMPARE(outputString, result);
    delete[] testCharacters;
    delete decoder;
}

void TerminalCharacterDecoderTest::testHTMLDecoder_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QVector<RenditionFlags>>("renditions");
    QTest::addColumn<QString>("result");

    /* Notes:
     * TODO: need to add foregroundColor, backgroundColor, and isRealCharacter
     */
    QTest::newRow("simple text with default rendition") << "hello" << QVector<RenditionFlags>(6).fill(DEFAULT_RENDITION) <<  R"(<span style="font-family:monospace"><span style="color:#000000;background-color:#ffffff;">hello</span><br></span>)";
    QTest::newRow("simple text with bold rendition") << "hello" << QVector<RenditionFlags>(6).fill(RE_BOLD) <<  R"(<span style="font-family:monospace"><span style="font-weight:bold;color:#000000;background-color:#ffffff;">hello</span><br></span>)";
    // The below is wrong; only the first rendition is used (eg ignores the |)
    QTest::newRow("simple text with underline and italic rendition") << "hello" << QVector<RenditionFlags>(6).fill(RE_UNDERLINE|RE_ITALIC) <<  R"(<span style="font-family:monospace"><span style="font-decoration:underline;color:#000000;background-color:#ffffff;">hello</span><br></span>)";

    QTest::newRow("text with &") << "hello &there" << QVector<RenditionFlags>(6).fill(DEFAULT_RENDITION) <<  R"(<span style="font-family:monospace"><span style="color:#000000;background-color:#ffffff;">hello &amp;there</span><br></span>)";
}

void TerminalCharacterDecoderTest::testHTMLDecoder()
{
    QFETCH(QString, text);
    QFETCH(QVector<RenditionFlags>, renditions);
    QFETCH(QString, result);

    TerminalCharacterDecoder *decoder = new HTMLDecoder();
    auto testCharacters = convertToCharacter(text, renditions);
    QString outputString;
    QTextStream outputStream(&outputString);
    decoder->begin(&outputStream);
    decoder->decodeLine(testCharacters, text.size(), /* ignored */ LINE_DEFAULT);
    decoder->end();
    QCOMPARE(outputString, result);
    delete[] testCharacters;
    delete decoder;
}

QTEST_GUILESS_MAIN(TerminalCharacterDecoderTest)
