/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALCOLOR_HPP
#define TERMINALCOLOR_HPP

// Qt
#include <QWidget>
#include <QColor>

// Konsole
#include "profile/Profile.h"
#include "characters/CharacterColor.h"
#include "konsoleprivate_export.h"

namespace Konsole
{

    class Profile;
    class ColorScheme;

    class KONSOLEPRIVATE_EXPORT TerminalColor : public QWidget
    {
        Q_OBJECT
    public:
        explicit TerminalColor(QWidget *parent);

        void applyProfile(const Profile::Ptr &profile, ColorScheme const *colorScheme, int randomSeed);

        QColor backgroundColor() const;
        QColor foregroundColor() const;
        void setColorTable(const ColorEntry *table);
        const ColorEntry *colorTable() const;

        void setOpacity(qreal opacity);

        void visualBell();

        qreal opacity() const;
        QRgb blendColor() const;

        QColor cursorColor() const
        {
            return m_cursorColor;
        }

        QColor cursorTextColor() const
        {
            return m_cursorTextColor;
        }

    public Q_SLOTS:
        void setBackgroundColor(const QColor &color);
        void setForegroundColor(const QColor &color);

    Q_SIGNALS:
        void onPalette(const QPalette &);

    protected:
        bool event(QEvent *event) override;
        void onColorsChanged();

    private Q_SLOTS:
        void swapFGBGColors();

    private:
        qreal m_opacity;
        QRgb m_blendColor;

        QColor m_cursorColor;
        QColor m_cursorTextColor;

        ColorEntry m_colorTable[TABLE_COLORS];
    };
}

#endif
