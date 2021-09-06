/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2012-2020 Kurt Hindenburg <kurt.hindenburg@gmail.com>
    SPDX-FileCopyrightText: 2020-2020 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    typedef std::function<void(QPainter &, bool)> pPrintContent;
    typedef std::function<QFont()> pVTFontGet;
    typedef std::function<void(const QFont)> pVTFontSet;
    typedef std::function<void(QPainter &painter, const QRect &rect, const QColor &backgroundColor, bool useOpacitySetting)> pDrawBackground;
    typedef std::function<void(QPainter &paint, const QRect &rect, bool friendly)> pDrawContents;
    typedef std::function<QColor()> pColorGet;

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
