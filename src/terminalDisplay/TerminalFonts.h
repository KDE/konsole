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
    QWidget *m_parent = nullptr;
    uint m_lineSpacing = 0;
    int m_fontHeight = 1;
    int m_fontWidth = 1;
    int m_fontAscent = 1;
    bool m_boldIntense = false;
    bool m_antialiasText = true;
    bool m_useFontLineCharacters = false;

    Profile::Ptr m_profile;
};

}

#endif
