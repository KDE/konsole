/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
