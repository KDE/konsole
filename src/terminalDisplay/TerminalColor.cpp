/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "TerminalColor.h"

// Konsole
#include "colorscheme/ColorScheme.h"

// Qt
#include <QApplication>
#include <QPalette>
#include <QTimer>

namespace Konsole
{
TerminalColor::TerminalColor(QObject *parent)
    : QObject(parent)
    , m_opacity(1.0)
    , m_blendColor(qRgba(0, 0, 0, 0xff))
    , m_cursorColor(QColor())
    , m_cursorTextColor(QColor())
{
    setColorTable(ColorScheme::defaultTable);
}

void TerminalColor::applyProfile(const Profile::Ptr &profile, ColorScheme const *colorScheme, uint randomSeed)
{
    QColor table[TABLE_COLORS];
    colorScheme->getColorTable(table, randomSeed);
    setColorTable(table);
    setOpacity(colorScheme->opacity());

    m_cursorColor = profile->useCustomCursorColor() ? profile->customCursorColor() : QColor();
    m_cursorTextColor = profile->useCustomCursorColor() ? profile->customCursorTextColor() : QColor();
}

QColor TerminalColor::backgroundColor() const
{
    return m_colorTable[DEFAULT_BACK_COLOR];
}

QColor TerminalColor::foregroundColor() const
{
    return m_colorTable[DEFAULT_FORE_COLOR];
}

void TerminalColor::setColorTable(const QColor *table)
{
    std::copy(table, table + TABLE_COLORS, m_colorTable);
    setBackgroundColor(m_colorTable[DEFAULT_BACK_COLOR]);
    onColorsChanged();
}

const QColor *TerminalColor::colorTable() const
{
    return m_colorTable;
}

void TerminalColor::setOpacity(qreal opacity)
{
    QColor color(m_blendColor);
    color.setAlphaF(opacity);
    m_opacity = opacity;

    m_blendColor = color.rgba();
    onColorsChanged();
}

void TerminalColor::visualBell()
{
    swapFGBGColors();
    QTimer::singleShot(200, this, &TerminalColor::swapFGBGColors);
}

qreal TerminalColor::opacity() const
{
    return m_opacity;
}

QRgb TerminalColor::blendColor() const
{
    return m_blendColor;
}

void TerminalColor::setBackgroundColor(const QColor &color)
{
    m_colorTable[DEFAULT_BACK_COLOR] = color;
    onColorsChanged();
}

void TerminalColor::setForegroundColor(const QColor &color)
{
    m_colorTable[DEFAULT_FORE_COLOR] = color;
    onColorsChanged();
}

void TerminalColor::onColorsChanged()
{
    QPalette palette = QApplication::palette();

    QColor buttonTextColor = m_colorTable[DEFAULT_FORE_COLOR];
    QColor backgroundColor = m_colorTable[DEFAULT_BACK_COLOR];
    backgroundColor.setAlphaF(m_opacity);

    QColor buttonColor = backgroundColor.toHsv();
    if (buttonColor.valueF() < 0.5) {
        buttonColor = buttonColor.lighter();
    } else {
        buttonColor = buttonColor.darker();
    }
    palette.setColor(QPalette::Button, buttonColor);
    palette.setColor(QPalette::Window, backgroundColor);
    palette.setColor(QPalette::Base, backgroundColor);
    palette.setColor(QPalette::WindowText, buttonTextColor);
    palette.setColor(QPalette::ButtonText, buttonTextColor);

    Q_EMIT onPalette(palette);
}

void TerminalColor::swapFGBGColors()
{
    QColor color = m_colorTable[DEFAULT_BACK_COLOR];
    m_colorTable[DEFAULT_BACK_COLOR] = m_colorTable[DEFAULT_FORE_COLOR];
    m_colorTable[DEFAULT_FORE_COLOR] = color;

    onColorsChanged();
}

}
