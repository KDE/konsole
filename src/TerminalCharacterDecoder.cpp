/*
    This file is part of Konsole, an X terminal.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Lesser General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "TerminalCharacterDecoder.h"

// Qt
#include <QtCore/QTextStream>
#include <KDebug>

// Konsole
#include "konsole_wcwidth.h"
#include "ExtendedCharTable.h"
#include "ColorScheme.h"

using namespace Konsole;
PlainTextDecoder::PlainTextDecoder()
    : _output(0)
    , _includeTrailingWhitespace(true)
    , _recordLinePositions(false)
{
}
void PlainTextDecoder::setTrailingWhitespace(bool enable)
{
    _includeTrailingWhitespace = enable;
}
bool PlainTextDecoder::trailingWhitespace() const
{
    return _includeTrailingWhitespace;
}
void PlainTextDecoder::begin(QTextStream* output)
{
    _output = output;
    if (!_linePositions.isEmpty())
        _linePositions.clear();
}
void PlainTextDecoder::end()
{
    _output = 0;
}

void PlainTextDecoder::setRecordLinePositions(bool record)
{
    _recordLinePositions = record;
}
QList<int> PlainTextDecoder::linePositions() const
{
    return _linePositions;
}
void PlainTextDecoder::decodeLine(const Character* const characters, int count, LineProperty /*properties*/
                                 )
{
    Q_ASSERT(_output);

    if (_recordLinePositions && _output->string()) {
        int pos = _output->string()->count();
        _linePositions << pos;
    }

    //TODO should we ignore or respect the LINE_WRAPPED line property?

    //note:  we build up a QString and send it to the text stream rather writing into the text
    //stream a character at a time because it is more efficient.
    //(since QTextStream always deals with QStrings internally anyway)
    QString plainText;
    plainText.reserve(count);

    int outputCount = count;

    // if inclusion of trailing whitespace is disabled then find the end of the
    // line
    if (!_includeTrailingWhitespace) {
        for (int i = count - 1 ; i >= 0 ; i--) {
            if (!characters[i].isSpace())
                break;
            else
                outputCount--;
        }
    }

    // find out the last technically real character in the line
    int realCharacterGuard = -1;
    for (int i = count - 1 ; i >= 0 ; i--) {
        // FIXME: the special case of '\n' here is really ugly
        // Maybe the '\n' should be added after calling this method in
        // Screen::copyLineToStream()
        if (characters[i].isRealCharacter && characters[i].character != '\n') {
            realCharacterGuard = i;
            break;
        }
    }

    for (int i = 0; i < outputCount;) {
        if (characters[i].rendition & RE_EXTENDED_CHAR) {
            ushort extendedCharLength = 0;
            const ushort* chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
            if (chars) {
                const QString s = QString::fromUtf16(chars, extendedCharLength);
                plainText.append(s);
                i += qMax(1, string_width(s));
            }
        } else {
            // All characters which appear before the last real character are
            // seen as real characters, even when they are technically marked as
            // non-real.
            //
            // This feels tricky, but otherwise leading "whitespaces" may be
            // lost in some situation. One typical example is copying the result
            // of `dialog --infobox "qwe" 10 10` .
            if (characters[i].isRealCharacter || i <= realCharacterGuard) {
                plainText.append(QChar(characters[i].character));
                i += qMax(1, konsole_wcwidth(characters[i].character));
            } else {
                ++i;  // should we 'break' directly here?
            }
        }
    }
    *_output << plainText;
}

HTMLDecoder::HTMLDecoder() :
    _output(0)
    , _colorTable(ColorScheme::defaultTable)
    , _innerSpanOpen(false)
    , _lastRendition(DEFAULT_RENDITION)
{
}

void HTMLDecoder::begin(QTextStream* output)
{
    _output = output;

    QString text;

    text.append("<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Strict//EN\"\n");
    text.append("\"http://www.w3.org/TR/xhtml1/DTD/xhtml1-strict.dtd\">\n");
    text.append("<html xmlns=\"http://www.w3.org/1999/xhtml\" lang=\"en\" xml:lang=\"en\">\n");
    text.append("<head>\n");
    text.append("<title>Konsole output</title>\n");
    text.append("<meta http-equiv=\"Content-Type\" content=\"text/html;charset=utf-8\" />\n");
    text.append("</head>\n");
    text.append("<body>\n");
    text.append("<div>\n");

    //open monospace span
    openSpan(text, "font-family:monospace");

    *output << text;
}

void HTMLDecoder::end()
{
    Q_ASSERT(_output);

    QString text;

    closeSpan(text);
    text.append("</div>\n");
    text.append("</body>\n");
    text.append("</html>\n");

    *_output << text;

    _output = 0;
}

//TODO: Support for LineProperty (mainly double width , double height)
void HTMLDecoder::decodeLine(const Character* const characters, int count, LineProperty /*properties*/
                            )
{
    Q_ASSERT(_output);

    QString text;

    int spaceCount = 0;

    for (int i = 0; i < count; i++) {
        //check if appearance of character is different from previous char
        if (characters[i].rendition != _lastRendition  ||
                characters[i].foregroundColor != _lastForeColor  ||
                characters[i].backgroundColor != _lastBackColor) {
            if (_innerSpanOpen) {
                closeSpan(text);
                _innerSpanOpen = false;
            }

            _lastRendition = characters[i].rendition;
            _lastForeColor = characters[i].foregroundColor;
            _lastBackColor = characters[i].backgroundColor;

            //build up style string
            QString style;

            bool useBold;

            //colors - a color table must have been defined first
            if (_colorTable) {
                ColorEntry::FontWeight weight = characters[i].fontWeight(_colorTable);
                if (weight == ColorEntry::UseCurrentFormat)
                    useBold = _lastRendition & RE_BOLD;
                else
                    useBold = weight == ColorEntry::Bold;

                if (useBold)
                    style.append("font-weight:bold;");

                if (_lastRendition & RE_UNDERLINE)
                    style.append("font-decoration:underline;");

                style.append(QString("color:%1;").arg(_lastForeColor.color(_colorTable).name()));

                style.append(QString("background-color:%1;").arg(_lastBackColor.color(_colorTable).name()));
            }

            //open the span with the current style
            openSpan(text, style);
            _innerSpanOpen = true;
        }

        //handle whitespace
        if (characters[i].isSpace())
            spaceCount++;
        else
            spaceCount = 0;

        //output current character
        if (spaceCount < 2) {
            if (characters[i].rendition & RE_EXTENDED_CHAR) {
                ushort extendedCharLength = 0;
                const ushort* chars = ExtendedCharTable::instance.lookupExtendedChar(characters[i].character, extendedCharLength);
                if (chars) {
                    text.append(QString::fromUtf16(chars, extendedCharLength));
                }
            } else {
                //escape HTML tag characters and just display others as they are
                const QChar ch = characters[i].character;
                if (ch == '<')
                    text.append("&lt;");
                else if (ch == '>')
                    text.append("&gt;");
                else
                    text.append(ch);
            }
        } else {
            // HTML truncates multiple spaces, so use a space marker instead
            // Use &#160 instead of &nbsp so xmllint will work.
            text.append("&#160;");
        }
    }

    //close any remaining open inner spans
    if (_innerSpanOpen) {
        closeSpan(text);
        _innerSpanOpen = false;
    }

    //start new line
    text.append("<br />");

    *_output << text;
}
void HTMLDecoder::openSpan(QString& text , const QString& style)
{
    text.append(QString("<span style=\"%1\">").arg(style));
}

void HTMLDecoder::closeSpan(QString& text)
{
    text.append("</span>");
}

void HTMLDecoder::setColorTable(const ColorEntry* table)
{
    _colorTable = table;
}
