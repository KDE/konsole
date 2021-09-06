/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef TERMINALFONTS_H
#define TERMINALFONTS_H

#include <QWidget>

#include "profile/Profile.h"

class QFont;

namespace Konsole
{
class TerminalFont
{
public:
    explicit TerminalFont(QWidget *parent = nullptr);
    ~TerminalFont() = default;

    void applyProfile(const Profile::Ptr &profile);

    void setVTFont(const QFont &f);
    QFont getVTFont() const;

    void increaseFontSize();
    void decreaseFontSize();
    void resetFontSize();

    void setLineSpacing(uint);
    uint lineSpacing() const;

    int fontHeight() const;
    int fontWidth() const;
    int fontAscent() const;
    bool boldIntense() const;
    bool antialiasText() const;
    bool useFontLineCharacters() const;

protected:
    void fontChange(const QFont &);

private:
    QWidget *m_parent;
    uint m_lineSpacing;
    int m_fontHeight;
    int m_fontWidth;
    int m_fontAscent;
    bool m_boldIntense;
    bool m_antialiasText;
    bool m_useFontLineCharacters;

    Profile::Ptr m_profile;
};

}

#endif
