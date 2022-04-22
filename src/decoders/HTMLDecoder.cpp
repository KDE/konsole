/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HTMLDecoder.h"

// Konsole colorScheme
#include <ColorSchemeManager.h>

// Konsole characters
#include <ExtendedCharTable.h>

// Qt
#include <QTextStream>

using namespace Konsole;

HTMLDecoder::HTMLDecoder(const QString &colorSchemeName, const QFont &profileFont)
    : _output(nullptr)
    , _colorSchemeName(colorSchemeName)
    , _profileFont(profileFont)
    , _innerSpanOpen(false)
    , _lastRendition(DEFAULT_RENDITION)
    , _lastForeColor(CharacterColor())
    , _lastBackColor(CharacterColor())
    , _validProfile(false)
{
    std::shared_ptr<const ColorScheme> colorScheme = nullptr;

    if (!colorSchemeName.isEmpty()) {
        colorScheme = ColorSchemeManager::instance()->findColorScheme(colorSchemeName);

        if (colorScheme == nullptr) {
            colorScheme = ColorSchemeManager::instance()->defaultColorScheme();
        }

        _validProfile = true;
    }

    if (colorScheme != nullptr) {
        colorScheme->getColorTable(_colorTable);
    } else {
        std::copy_n(ColorScheme::defaultTable, TABLE_COLORS, _colorTable);
    }
}

void HTMLDecoder::begin(QTextStream *output)
{
    _output = output;

    if (_validProfile) {
        QString style;

        style.append(QStringLiteral("font-family:'%1',monospace;").arg(_profileFont.family()));

        // Prefer point size if set
        if (_profileFont.pointSizeF() > 0) {
            style.append(QStringLiteral("font-size:%1pt;").arg(_profileFont.pointSizeF()));
        } else {
            style.append(QStringLiteral("font-size:%1px;").arg(_profileFont.pixelSize()));
        }

        style.append(QStringLiteral("color:%1;").arg(_colorTable[DEFAULT_FORE_COLOR].name()));
        style.append(QStringLiteral("background-color:%1;").arg(_colorTable[DEFAULT_BACK_COLOR].name()));

        *output << QStringLiteral("<body style=\"%1\">").arg(style);
    } else {
        QString text;
        openSpan(text, QStringLiteral("font-family:monospace"));
        *output << text;
    }
}

void HTMLDecoder::end()
{
    Q_ASSERT(_output);

    if (_validProfile) {
        *_output << QStringLiteral("</body>");
    } else {
        QString text;
        closeSpan(text);
        *_output << text;
    }

    _output = nullptr;
}

// TODO: Support for LineProperty (mainly double width , double height)
void HTMLDecoder::decodeLine(const Character *const characters, int count, LineProperty /*properties*/)
{
    Q_ASSERT(_output);

    QString text;

    int spaceCount = 0;

    for (int i = 0; i < count; i++) {
        // check if appearance of character is different from previous char
        if (characters[i].rendition != _lastRendition || characters[i].foregroundColor != _lastForeColor || characters[i].backgroundColor != _lastBackColor) {
            if (_innerSpanOpen) {
                closeSpan(text);
                _innerSpanOpen = false;
            }

            _lastRendition = characters[i].rendition;
            _lastForeColor = characters[i].foregroundColor;
            _lastBackColor = characters[i].backgroundColor;

            // build up style string
            QString style;

            bool useBold = (_lastRendition & RE_BOLD) != 0;
            if (useBold) {
                style.append(QLatin1String("font-weight:bold;"));
            }

            if ((_lastRendition & RE_UNDERLINE) != 0) {
                style.append(QLatin1String("font-decoration:underline;"));
            }

            style.append(QStringLiteral("color:%1;").arg(_lastForeColor.color(_colorTable).name()));

            style.append(QStringLiteral("background-color:%1;").arg(_lastBackColor.color(_colorTable).name()));

            // open the span with the current style
            openSpan(text, style);
            _innerSpanOpen = true;
        }

        // handle whitespace
        if (characters[i].isSpace()) {
            spaceCount++;
        } else {
            spaceCount = 0;
        }

        // output current character
        if (spaceCount < 2) {
            if ((characters[i].rendition & RE_EXTENDED_CHAR) != 0) {
                ushort extendedCharLength = 0;
                const uint *chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
                if (chars != nullptr) {
                    text.append(QString::fromUcs4(chars, extendedCharLength));
                }
            } else {
                // escape HTML tag characters and just display others as they are
                const QChar ch(characters[i].character);
                if (ch == QLatin1Char('<')) {
                    text.append(QLatin1String("&lt;"));
                } else if (ch == QLatin1Char('>')) {
                    text.append(QLatin1String("&gt;"));
                } else if (ch == QLatin1Char('&')) {
                    text.append(QLatin1String("&amp;"));
                } else {
                    text.append(ch);
                }
            }
        } else {
            // HTML truncates multiple spaces, so use a space marker instead
            // Use &#160 instead of &nbsp so xmllint will work.
            text.append(QLatin1String("&#160;"));
        }
    }

    // close any remaining open inner spans
    if (_innerSpanOpen) {
        closeSpan(text);
        _innerSpanOpen = false;
    }

    // start new line
    text.append(QLatin1String("<br>"));

    *_output << text;
}

void HTMLDecoder::openSpan(QString &text, const QString &style)
{
    text.append(QStringLiteral("<span style=\"%1\">").arg(style));
}

void HTMLDecoder::closeSpan(QString &text)
{
    text.append(QLatin1String("</span>"));
}
