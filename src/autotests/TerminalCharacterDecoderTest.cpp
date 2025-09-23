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
#include "colorscheme/ColorScheme.h"

// Qt
#include <QTextStream>

// KDE
#include <QTest>

using namespace Konsole;

void TerminalCharacterDecoderTest::convertToCharacter(Character *charResult, const QString &text, QVector<RenditionFlags> renditions)
{
    // Force renditions size to match that of text; default will be DEFAULT_RENDITION.
    if (renditions.size() < text.size()) {
        renditions.resize(text.size());
    }
    for (int i = 0; i < text.size(); ++i) {
        charResult[i] = Character(text.at(i).unicode());
        charResult[i].rendition.all = renditions.at(i);
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
    QTest::newRow("simple text with underline and italic rendition") << "hello" << QVector<RenditionFlags>(6).fill(RE_UNDERLINE_BIT | RE_ITALIC) << "hello";

    QTest::newRow("simple text with default rendition (shorten)") << "hello" << QVector<RenditionFlags>(4).fill(DEFAULT_RENDITION) << "hello";
    QTest::newRow("simple text with underline rendition (shorten)") << "hello" << QVector<RenditionFlags>(4).fill(RE_UNDERLINE_BIT) << "hello";
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
    decoder->decodeLine(testCharacters, text.size(), /* ignored */ LineProperty());
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
        << "hello" << QVector<RenditionFlags>(6).fill(RE_UNDERLINE_BIT | RE_ITALIC)
        << R"(<span style="font-family:monospace"><span style="text-decoration:underline;color:#000000;background-color:#ffffff;">hello</span><br></span>)";

    QTest::newRow("text with &")
        << "hello &there" << QVector<RenditionFlags>(6).fill(DEFAULT_RENDITION)
        << R"(<span style="font-family:monospace"><span style="color:#000000;background-color:#ffffff;">hello &amp;there</span><br></span>)";

    // In test input, '\n' marks line breaks to represent multi-line cases.
    QString inputLine1 = QString(40, QLatin1Char('A'));
    QString inputLine2 = QString(10, QLatin1Char('B'));
    QString inputText = inputLine1 + QLatin1Char('\n') + inputLine2;

    QTest::newRow("multi-line with bold style")
        << inputText << QVector<RenditionFlags>(51).fill(RE_BOLD)
        << QStringLiteral(
               R"(<span style="font-family:monospace"><span style="font-weight:bold;color:#000000;background-color:#ffffff;">%1</span><br><span style="font-weight:bold;color:#000000;background-color:#ffffff;">%2</span><br></span>)")
               .arg(inputLine1, inputLine2);
}

void TerminalCharacterDecoderTest::testHTMLDecoder()
{
    QFETCH(QString, text);
    QFETCH(QVector<RenditionFlags>, renditions);
    QFETCH(QString, result);

    TerminalCharacterDecoder *decoder = new HTMLDecoder(ColorScheme::defaultTable);
    auto testCharacters = new Character[text.size()];
    convertToCharacter(testCharacters, text, renditions);
    QString outputString;
    QTextStream outputStream(&outputString);
    decoder->begin(&outputStream);

    // Split input text into multiple lines at '\n', and decode each line.
    QStringList lines = text.split(QLatin1Char('\n'));
    int pos = 0;
    for (const QString &line : lines) {
        decoder->decodeLine(testCharacters + pos, line.size(), /* ignored */ LineProperty());
        pos += line.size() + 1; // +1 for the '\n'
    }

    decoder->end();
    delete[] testCharacters;
    delete decoder;

    // ensure we exported the encoding, bug 500515
    QVERIFY(outputString.contains(QStringLiteral("<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />")));

    // strip HTML document
    outputString.replace(QRegularExpression(QStringLiteral("^.*<body>\\n"), QRegularExpression::DotMatchesEverythingOption), QString());
    outputString.replace(QRegularExpression(QStringLiteral("</body>.*$"), QRegularExpression::DotMatchesEverythingOption), QString());

    QCOMPARE(outputString, result);
}

QTEST_GUILESS_MAIN(TerminalCharacterDecoderTest)

#include "moc_TerminalCharacterDecoderTest.cpp"
