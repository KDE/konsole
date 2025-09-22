/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HTMLDECODER_H
#define HTMLDECODER_H

// Konsole decoders
#include "TerminalCharacterDecoder.h"

#include <QFont>
class QString;

namespace Konsole
{
/**
 * A terminal character decoder which produces pretty HTML markup
 */
class HTMLDecoder : public TerminalCharacterDecoder
{
public:
    /**
     * Constructs an HTML decoder using a default black-on-white color scheme.
     */
    explicit HTMLDecoder(const QColor *colorTable);

    void decodeLine(const Character *const characters, int count, LineProperty properties) override;

    void begin(QTextStream *output) override;
    void end() override;

private:
    void openSpan(QString &text, const QString &style);
    void closeSpan(QString &text);

    QTextStream *_output;
    QColor _colorTable[TABLE_COLORS];
    bool _innerSpanOpen;
    RenditionFlags _lastRendition;
    CharacterColor _lastForeColor;
    CharacterColor _lastBackColor;
    QString _style;
};
}

#endif
