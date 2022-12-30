/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALPAINTER_HPP
#define TERMINALPAINTER_HPP

// Qt
#include <QVector>

// Konsole
#include "../characters/Character.h"
#include "Enumeration.h"
#include "ScreenWindow.h"
#include "colorscheme/ColorSchemeWallpaper.h"
#include "profile/Profile.h"
#include "terminalDisplay/TerminalDisplay.h"

class QRect;
class QColor;
class QRegion;
class QPainter;
class QString;
class QTimer;

namespace Konsole
{
class Character;
class TerminalDisplay;

class TerminalPainter : public QObject
{
public:
    explicit TerminalPainter(TerminalDisplay *parentDisplay);
    ~TerminalPainter() override = default;

public Q_SLOTS:
    // -- Drawing helpers --

    // divides the part of the display specified by 'rect' into
    // fragments according to their colors and styles and calls
    // drawTextFragment() or drawPrinterFriendlyTextFragment()
    // to draw the fragments
    void drawContents(Character *image,
                      QPainter &paint,
                      const QRect &rect,
                      bool PrinterFriendly,
                      int imageSize,
                      bool bidiEnabled,
                      QVector<LineProperty> lineProperties,
                      CharacterColor const *ulColorTable = nullptr);

    // draw a transparent rectangle over the line of the current match
    void drawCurrentResultRect(QPainter &painter, const QRect &searchResultRect);

    // draw a thin highlight on the left of the screen for lines that have been scrolled into view
    void highlightScrolledLines(QPainter &painter, bool isTimerActive, QRect rect);

    // compute which region need to be repainted for scrolled lines highlight
    QRegion highlightScrolledLinesRegion(TerminalScrollBar *scrollBar);

    // draws the background for a text fragment
    // if useOpacitySetting is true then the color's alpha value will be set to
    // the display's transparency (set with setOpacity()), otherwise the background
    // will be drawn fully opaque
    void drawBackground(QPainter &painter, const QRect &rect, const QColor &backgroundColor, bool useOpacitySetting);

    // draws the characters or line graphics in a text fragment
    void drawCharacters(QPainter &painter,
                        const QRect &rect,
                        const QString &text,
                        const Character style,
                        const QColor &characterColor,
                        const LineProperty lineProperty);

    // draws the preedit string for input methods
    void drawInputMethodPreeditString(QPainter &painter, const QRect &rect, TerminalDisplay::InputMethodData &inputMethodData, Character *image);

private:
    // draws a string of line graphics
    void drawLineCharString(TerminalDisplay *display, QPainter &painter, int x, int y, const QString &str, const Character attributes);

    void updateCursorTextColor(const QColor &backgroundColor, QColor &characterColor);
    // draws the cursor character
    void drawCursor(QPainter &painter, const QRect &rect, const QColor &foregroundColor, const QColor &backgroundColor, QColor &characterColor);

    TerminalDisplay *m_parentDisplay = nullptr;
    void drawBelowText(QPainter &painter,
                       const QRect &rect,
                       Character *style,
                       int startX,
                       int width,
                       int fontWidth,
                       const QColor *colorTable,
                       const bool invertedRendition,
                       int *vis2line,
                       int *line2log,
                       bool bidiEnabled,
                       int lastNonSpace,
                       QColor background);
    void drawAboveText(QPainter &painter,
                       const QRect &rect,
                       Character *style,
                       int startX,
                       int width,
                       int fontWidth,
                       const QColor *colorTable,
                       const bool invertedRendition,
                       int *vis2line,
                       int *line2log,
                       bool bidiEnabled,
                       int lastNonSpace,
                       CharacterColor const *ulColorTable);
    void drawImagesBelowText(QPainter &painter, const QRect &rect, int fontWidth, int fontHeight, int &placementIdx);
    void drawImagesAboveText(QPainter &painter, const QRect &rect, int fontWidth, int fontHeight, int &placementIdx);

    void drawTextCharacters(QPainter &painter,
                            const QRect &rect,
                            const QString &text,
                            Character style,
                            const QColor *colorTable,
                            const bool invertedRendition,
                            const LineProperty lineProperty,
                            bool printerFriendly,
                            RenditionFlags &oldRendition,
                            QColor oldColor,
                            QFont::Weight normalWeight,
                            QFont::Weight boldWeight);
};

}

#endif
