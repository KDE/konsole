/*
 *  This file is part of Konsole, a terminal emulator for KDE.
 *
 *  Copyright 2012  Frederik Gladhorn <gladhorn@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301  USA.
 */

#ifndef TERMINALDISPLAYACCESSIBLE_H
#define TERMINALDISPLAYACCESSIBLE_H

//#include <QtGlobal>

#include <QAccessibleTextInterface>
#include <QAccessibleWidget>

#include "TerminalDisplay.h"
#include "ScreenWindow.h"
#include "Screen.h"

namespace Konsole {
/**
 * Class implementing the QAccessibleInterface for the terminal display.
 * This exposes information about the display to assistive technology using the QAccessible framework.
 *
 * Most functions are re-implemented from QAccessibleTextInterface.
 */
class TerminalDisplayAccessible : public QAccessibleWidget, public QAccessibleTextInterface
{
public:
    explicit TerminalDisplayAccessible(TerminalDisplay *display);

    QString text(QAccessible::Text t) const override;
    QString text(int startOffset, int endOffset) const override;
    int characterCount() const override;

    int selectionCount() const override;
    void selection(int selectionIndex, int *startOffset, int *endOffset) const override;
    void addSelection(int startOffset, int endOffset) override;
    void setSelection(int selectionIndex, int startOffset, int endOffset) override;
    void removeSelection(int selectionIndex) override;

    QRect characterRect(int offset) const override;
    int offsetAtPoint(const QPoint &point) const override;
    void scrollToSubstring(int startIndex, int endIndex) override;

    QString attributes(int offset, int *startOffset, int *endOffset) const override;

    int cursorPosition() const override;
    void setCursorPosition(int position) override;

    void *interface_cast(QAccessible::InterfaceType type) override;

private:
    Konsole::TerminalDisplay *display() const;

    inline int positionToOffset(int column, int line) const
    {
        return line * display()->_usedColumns + column;
    }

    inline int lineForOffset(int offset) const
    {
        return offset / display()->_usedColumns;
    }

    inline int columnForOffset(int offset) const
    {
        return offset % display()->_usedColumns;
    }

    QString visibleText() const;
};
} // namespace

#endif // TERMINALDISPLAYACCESSIBLE_H
