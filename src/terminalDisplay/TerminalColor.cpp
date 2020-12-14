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
#include <QTimer>
#include <QPalette>
#include <QApplication>

namespace Konsole
{

    TerminalColor::TerminalColor(QWidget *parent)
        : QWidget(parent)
        , m_opacity(1.0)
        , m_blendColor(qRgba(0, 0, 0, 0xff))
        , m_cursorColor(QColor())
        , m_cursorTextColor(QColor())
    {
        setColorTable(ColorScheme::defaultTable);
    }
    
    void TerminalColor::applyProfile(const Profile::Ptr &profile, ColorScheme const *colorScheme, int randomSeed)
    {
        ColorEntry table[TABLE_COLORS];
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
    
    void TerminalColor::setColorTable(const ColorEntry *table) 
    {
        for (int index = 0; index < TABLE_COLORS; index++) {
            m_colorTable[index] = table[index];
        }
        setBackgroundColor(m_colorTable[DEFAULT_BACK_COLOR]);
        onColorsChanged();
    }
    
    const ColorEntry* TerminalColor::colorTable() const
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
  
    bool TerminalColor::event(QEvent *event) 
    {
        switch (event->type()) {
            case QEvent::PaletteChange:
            case QEvent::ApplicationPaletteChange:
                onColorsChanged();
                break;
            
            default:
                break;
        }
        return QWidget::event(event);
    }
    
    void TerminalColor::onColorsChanged() 
    {
        QPalette palette = QApplication::palette();

        QColor buttonTextColor = m_colorTable[DEFAULT_FORE_COLOR];
        QColor backgroundColor = m_colorTable[DEFAULT_BACK_COLOR];
        backgroundColor.setAlpha(m_opacity);

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

        QWidget *widget = qobject_cast<QWidget*>(parent());

        widget->setPalette(palette);

        emit onPalette(palette);
        
        widget->update();
    }
    
    void TerminalColor::swapFGBGColors() 
    {
        ColorEntry color = m_colorTable[DEFAULT_BACK_COLOR];
        m_colorTable[DEFAULT_BACK_COLOR] = m_colorTable[DEFAULT_FORE_COLOR];
        m_colorTable[DEFAULT_FORE_COLOR] = color;

        onColorsChanged();
    }

}
