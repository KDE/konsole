/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HTMLDecoder.h"

// Konsole characters
#include <ExtendedCharTable.h>

// Qt
#include <QTextStream>

using namespace Konsole;

HTMLDecoder::HTMLDecoder(const QColor *colorTable)
    : _output(nullptr)
    , _innerSpanOpen(false)
    , _lastRendition(DEFAULT_RENDITION)
    , _lastForeColor(CharacterColor())
    , _lastBackColor(CharacterColor())
{
    Q_ASSERT(colorTable);
    std::copy_n(colorTable, TABLE_COLORS, _colorTable);
}

void HTMLDecoder::begin(QTextStream *output)
{
    _output = output;

    // open html document & body, ensure right encoding set, bug 500515
    *_output << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    *_output << "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\" \"DTD/xhtml1-strict.dtd\">\n";
    *_output << "<html xmlns=\"http://www.w3.org/1999/xhtml\">\n";
    *_output << "<head>\n";
    *_output << "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=UTF-8\" />\n";
    *_output << "</head>\n";
    *_output << "<body>\n";

    QString text;
    openSpan(text, QStringLiteral("font-family:monospace"));
    *output << text;
}

void HTMLDecoder::end()
{
    Q_ASSERT(_output);

    QString text;
    closeSpan(text);
    *_output << text;

    // close body & html document
    *_output << "</body>\n";
    *_output << "</html>\n";

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
        if (characters[i].rendition.all != _lastRendition || characters[i].foregroundColor != _lastForeColor
            || characters[i].backgroundColor != _lastBackColor) {
            if (_innerSpanOpen) {
                closeSpan(text);
                _innerSpanOpen = false;
            }

            _lastRendition = characters[i].rendition.all;
            _lastForeColor = characters[i].foregroundColor;
            _lastBackColor = characters[i].backgroundColor;

            // build up style string
            QString style;

            bool useBold = (_lastRendition & RE_BOLD) != 0;
            if (useBold) {
                style.append(QLatin1String("font-weight:bold;"));
            }

            if ((_lastRendition & RE_UNDERLINE_MASK) != 0) {
                style.append(QLatin1String("text-decoration:underline;"));
            }

            style.append(QStringLiteral("color:%1;").arg(_lastForeColor.color(_colorTable).name()));
            style.append(QStringLiteral("background-color:%1;").arg(_lastBackColor.color(_colorTable).name()));

            _style = style;
            // open the span with the current style
            openSpan(text, _style);
            _innerSpanOpen = true;
        } else if (i == 0) {
            openSpan(text, _style);
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
            if ((characters[i].rendition.all & RE_EXTENDED_CHAR) != 0) {
                ushort extendedCharLength = 0;
                const char32_t *chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
                if (chars != nullptr) {
                    text.append(QString::fromUcs4(chars, extendedCharLength));
                }
            } else {
                // escape HTML tag characters and just display others as they are
                const char32_t ch = characters[i].character;
                if (ch == U'<') {
                    text.append(QLatin1String("&lt;"));
                } else if (ch == U'>') {
                    text.append(QLatin1String("&gt;"));
                } else if (ch == U'&') {
                    text.append(QLatin1String("&amp;"));
                } else if (ch == U'\0') {
                    // do nothing for the right half of double-width character
                } else {
                    text.append(QString::fromUcs4(&characters[i].character, 1));
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
