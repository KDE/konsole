/*
    This file is part of Konsole, an X terminal.
    
    Copyright (C) 2006 by Robert Knight <robertknight@gmail.com>
    
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

// Qt
#include <QTextStream>

// KDE
#include <kdebug.h>

// Konsole
#include "TerminalCharacterDecoder.h"

using namespace Konsole;

PlainTextDecoder::PlainTextDecoder()
 : _includeTrailingWhitespace(true)
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

void PlainTextDecoder::decodeLine(const Character* const characters, int count, LineProperty /*properties*/,
							 QTextStream* output)
{
	//TODO should we ignore or respect the LINE_WRAPPED line property?

	//note:  we build up a QString and send it to the text stream rather writing into the text
	//stream a character at a time because it is more efficient.
	//(since QTextStream always deals with QStrings internally anyway)
	QString plainText;
	plainText.reserve(count);
   
    int outputCount = count;

    // if inclusion of trailing whitespace is disabled then find the end of the
    // line
    if ( !_includeTrailingWhitespace )
    {
        for (int i = count-1 ; i >= 0 ; i--)
        {
            if ( characters[i].character != ' '  )
                break;
            else
                outputCount--;
        }
    }

	for (int i=0;i<outputCount;i++)
	{
		plainText.append( QChar(characters[i].character) );
	}

	*output << plainText;
}

HTMLDecoder::HTMLDecoder() :
		colorTable(base_color_table)
{
	
}

//TODO: Support for LineProperty (mainly double width , double height)
void HTMLDecoder::decodeLine(const Character* const characters, int count, LineProperty /*properties*/,
							QTextStream* output)
{
	QString text;

	bool innerSpanOpen = false;
	int spaceCount = 0;
	UINT8 lastRendition = DEFAULT_RENDITION;
	CharacterColor lastForeColor;
	CharacterColor lastBackColor;
	
	//open monospace span
	openSpan(text,"font-family:monospace");
	
	for (int i=0;i<count;i++)
	{
		QChar ch(characters[i].character);

		//check if appearance of character is different from previous char
		if ( characters[i].rendition != lastRendition  ||
		     characters[i].foregroundColor != lastForeColor  ||
			 characters[i].backgroundColor != lastBackColor )
		{
			if ( innerSpanOpen )
					closeSpan(text);

			lastRendition = characters[i].rendition;
			lastForeColor = characters[i].foregroundColor;
			lastBackColor = characters[i].backgroundColor;
			
			//build up style string
			QString style;

			if ( lastRendition & RE_BOLD ||
                             (colorTable && characters[i].isBold(colorTable)) )
					style.append("font-weight:bold;");


			if ( lastRendition & RE_UNDERLINE )
					style.append("font-decoration:underline;");
		
			//colours - a colour table must have been defined first
			if ( colorTable )	
			{
				style.append( QString("color:%1;").arg(lastForeColor.color(colorTable).name() ) );

				if (!characters[i].isTransparent(colorTable))
				{
					style.append( QString("background-color:%1;").arg(lastBackColor.color(colorTable).name() ) );
				}
			}
		
			//open the span with the current style	
			openSpan(text,style);
			innerSpanOpen = true;
		}

		//handle whitespace
		if (ch.isSpace())
			spaceCount++;
		else
			spaceCount = 0;
		

		//output current character
		if (spaceCount < 2)
		{
			//escape HTML tag characters and just display others as they are
			if ( ch == '<' )
				text.append("&lt;");
			else if (ch == '>')
					text.append("&gt;");
			else	
					text.append(ch);
		}
		else
		{
			text.append("&nbsp;"); //HTML truncates multiple spaces, so use a space marker instead
		}
		
	}

	//close any remaining open inner spans
	if ( innerSpanOpen )
		closeSpan(text);

	//close monospace span
	closeSpan(text);
	
	//start new line
	text.append("<br>");
	
	*output << text;
}

void HTMLDecoder::openSpan(QString& text , const QString& style)
{
	text.append( QString("<span style=\"%1\">").arg(style) );
}

void HTMLDecoder::closeSpan(QString& text)
{
	text.append("</span>");
}

void HTMLDecoder::setColorTable(const ColorEntry* table)
{
	colorTable = table;
}
