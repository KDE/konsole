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

#include <QtGlobal>

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)

#include <QtGui/qaccessible.h>
#include <QtGui/qaccessible2.h>
#include <qaccessiblewidget.h>

#include "TerminalDisplay.h"
#include "ScreenWindow.h"
#include "Screen.h"

namespace Konsole
{
/**
 * Class implementing the QAccessibleInterface for the terminal display.
 * This exposes information about the display to assistive technology using the QAccessible framework.
 *
 * Most functions are re-implemented from QAccessibleTextInterface.
 */
class TerminalDisplayAccessible
    : public QAccessibleWidgetEx, public QAccessibleTextInterface, public QAccessibleSimpleEditableTextInterface
{
    Q_ACCESSIBLE_OBJECT

public:
    explicit TerminalDisplayAccessible(TerminalDisplay *display);

    QString text(QAccessible::Text t, int child) const;

    QString text(int startOffset, int endOffset);
    QString textAfterOffset(int offset, QAccessible2::BoundaryType boundaryType, int* startOffset, int* endOffset);
    QString textAtOffset(int offset, QAccessible2::BoundaryType boundaryType, int* startOffset, int* endOffset);
    QString textBeforeOffset(int offset, QAccessible2::BoundaryType boundaryType, int* startOffset, int* endOffset);
    int characterCount();

    int selectionCount();
    void selection(int selectionIndex, int *startOffset, int *endOffset);
    void addSelection(int startOffset, int endOffset);
    void setSelection(int selectionIndex, int startOffset, int endOffset);
    void removeSelection(int selectionIndex);

    QRect characterRect(int offset, QAccessible2::CoordinateType coordType);
    int offsetAtPoint(const QPoint& point, QAccessible2::CoordinateType coordType);
    void scrollToSubstring(int startIndex, int endIndex);

    QString attributes(int offset, int* startOffset, int* endOffset);

    int cursorPosition();
    void setCursorPosition(int position);

private:
    Konsole::TerminalDisplay *display();

    inline int positionToOffset(int column, int line) {
        return line * display()->_usedColumns + column;
    }

    inline int lineForOffset(int offset) {
        return offset / display()->_usedColumns;
    }

    inline int columnForOffset(int offset) {
        return offset % display()->_usedColumns;
    }

    QString visibleText() const;
};


} // namespace
#else
#pragma message("The accessibility code needs proper porting to Qt5")
#endif

#endif // TERMINALDISPLAYACCESSIBLE_H
