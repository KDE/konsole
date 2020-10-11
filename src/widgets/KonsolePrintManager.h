/*
    Copyright 2020-2020 by Gustavo Carneiro <gcarneiroa@hotmail.com>
    Copyright 2012-2020 by Kurt Hindenburg <kurt.hindenburg@gmail.com>
    Copyright 2020-2020 by Tomaz Canabrava <tcanabrava@kde.org>

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

#ifndef KONSOLEPRINTMANAGER_H
#define KONSOLEPRINTMANAGER_H

#include <functional>

class QFont;
class QRect;
class QColor;
class QPoint;
class QWidget;
class QPainter;

namespace Konsole
{
    class KonsolePrintManager
    {
    public:
        typedef std::function<void (QPainter&, bool)> pPrintContent;
        typedef std::function<QFont ()> pVTFontGet;
        typedef std::function<void (const QFont)> pVTFontSet;
        typedef std::function<void (QPainter &painter,
                                const QRect &rect,
                                const QColor &backgroundColor,
                                bool useOpacitySetting)> pDrawBackground;
        typedef std::function<void (QPainter &paint, const QRect &rect, bool friendly)> pDrawContents;
        typedef std::function<QColor ()> pColorGet;

        KonsolePrintManager(pDrawBackground drawBackground, pDrawContents drawContents, pColorGet colorGet);
        ~KonsolePrintManager() = default;

        void printRequest(pPrintContent pContent, QWidget *parent);
        void printContent(QPainter &painter, bool friendly, QPoint columnsLines, pVTFontGet vtFontGet, pVTFontSet vtFontSet);

    private:
        pDrawBackground _drawBackground;
        pDrawContents _drawContents;
        pColorGet _backgroundColor;
    };
}

#endif
