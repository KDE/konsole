/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2013, 2018 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "TerminalCharacterDecoderTest.h"

// Konsole
#include "../decoders/HTMLDecoder.h"
#include "../decoders/PlainTextDecoder.h"

// Qt
#include <QTextStream>

// KDE
#include <qtest.h>

using namespace Konsole;

void TerminalCharacterDecoderTest::convertToCharacter(Character *charResult, const QString &text, QVector<RenditionFlags> renditions)
{
    // Force renditions size to match that of text; default will be DEFAULT_RENDITION.
    if (renditions.size() < text.size()) {
        renditions.resize(text.size());
    }
    for (int i = 0; i < text.size(); ++i) {
        charResult[i] = Character(text.at(i).unicode());
        charResult[i].rendition = renditions.at(i);
    }
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
    QTest::newRow("simple text with underline and italic rendition") << "hello" << QVector<RenditionFlags>(6).fill(RE_UNDERLINE | RE_ITALIC) << "hello";

    QTest::newRow("simple text with default rendition (shorten)") << "hello" << QVector<RenditionFlags>(4).fill(DEFAULT_RENDITION) << "hello";
    QTest::newRow("simple text with underline rendition (shorten)") << "hello" << QVector<RenditionFlags>(4).fill(RE_UNDERLINE) << "hello";
}

void TerminalCharacterDecoderTest::testPlainTextDecoder()
{
    QFETCH(QString, text);
    QFETCH(QVector<RenditionFlags>, renditions);
    QFETCH(QString, result);

    TerminalCharacterDecoder *decoder = new PlainTextDecoder();
    auto testCharacters = new Character[text.size()];
    convertToCharacter(testCharacters, text, renditions);
    QString outputString;
    QTextStream outputStream(&outputString);
    decoder->begin(&outputStream);
    decoder->decodeLine(testCharacters, text.size(), /* ignored */ LINE_DEFAULT);
    decoder->end();
    delete[] testCharacters;
    delete decoder;
    QCOMPARE(outputString, result);
}

void TerminalCharacterDecoderTest::testHTMLDecoder_data()
{
    QTest::addColumn<QString>("text");
    QTest::addColumn<QVector<RenditionFlags>>("renditions");
    QTest::addColumn<QString>("result");

    /* Notes:
     * TODO: need to add foregroundColor, backgroundColor, and isRealCharacter
     */
    QTest::newRow("simple text with default rendition")
        << "hello" << QVector<RenditionFlags>(6).fill(DEFAULT_RENDITION)
        << R"(<span style="font-family:monospace"><span style="color:#000000;background-color:#ffffff;">hello</span><br></span>)";
    QTest::newRow("simple text with bold rendition")
        << "hello" << QVector<RenditionFlags>(6).fill(RE_BOLD)
        << R"(<span style="font-family:monospace"><span style="font-weight:bold;color:#000000;background-color:#ffffff;">hello</span><br></span>)";
    // The below is wrong; only the first rendition is used (eg ignores the |)
    QTest::newRow("simple text with underline and italic rendition")
        << "hello" << QVector<RenditionFlags>(6).fill(RE_UNDERLINE | RE_ITALIC)
        << R"(<span style="font-family:monospace"><span style="font-decoration:underline;color:#000000;background-color:#ffffff;">hello</span><br></span>)";

    QTest::newRow("text with &")
        << "hello &there" << QVector<RenditionFlags>(6).fill(DEFAULT_RENDITION)
        << R"(<span style="font-family:monospace"><span style="color:#000000;background-color:#ffffff;">hello &amp;there</span><br></span>)";
}

void TerminalCharacterDecoderTest::testHTMLDecoder()
{
    QFETCH(QString, text);
    QFETCH(QVector<RenditionFlags>, renditions);
    QFETCH(QString, result);

    TerminalCharacterDecoder *decoder = new HTMLDecoder();
    auto testCharacters = new Character[text.size()];
    convertToCharacter(testCharacters, text, renditions);
    QString outputString;
    QTextStream outputStream(&outputString);
    decoder->begin(&outputStream);
    decoder->decodeLine(testCharacters, text.size(), /* ignored */ LINE_DEFAULT);
    decoder->end();
    delete[] testCharacters;
    delete decoder;
    QCOMPARE(outputString, result);
}

QTEST_GUILESS_MAIN(TerminalCharacterDecoderTest)
