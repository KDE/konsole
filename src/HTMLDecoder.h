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

#ifndef HTMLDECODER_H
#define HTMLDECODER_H

// Konsole
#include "TerminalCharacterDecoder.h"

#include "profile/Profile.h"

namespace Konsole
{
    /**
     * A terminal character decoder which produces pretty HTML markup
     */
    class KONSOLEPRIVATE_EXPORT HTMLDecoder : public TerminalCharacterDecoder
    {
    public:
        /**
         * Constructs an HTML decoder using a default black-on-white color scheme.
         */
        explicit HTMLDecoder(const Profile::Ptr &profile = Profile::Ptr());

        void decodeLine(const Character * const characters, int count,
                        LineProperty properties) override;

        void begin(QTextStream *output) override;
        void end() override;

    private:
        void openSpan(QString &text, const QString &style);
        void closeSpan(QString &text);

        QTextStream *_output;
        Profile::Ptr _profile;
        ColorEntry _colorTable[TABLE_COLORS];
        bool _innerSpanOpen;
        RenditionFlags _lastRendition;
        CharacterColor _lastForeColor;
        CharacterColor _lastBackColor;
    };
}

#endif
