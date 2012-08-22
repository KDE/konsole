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

#include "TerminalDisplayAccessible.h"

#if QT_VERSION >= 0x040800 // added in Qt 4.8.0
QString Q_GUI_EXPORT qTextBeforeOffsetFromString(int offset, QAccessible2::BoundaryType boundaryType,
        int* startOffset, int* endOffset, const QString& text);
QString Q_GUI_EXPORT qTextAtOffsetFromString(int offset, QAccessible2::BoundaryType boundaryType,
        int* startOffset, int* endOffset, const QString& text);
QString Q_GUI_EXPORT qTextAfterOffsetFromString(int offset, QAccessible2::BoundaryType boundaryType,
        int* startOffset, int* endOffset, const QString& text);
#endif

using namespace Konsole;

TerminalDisplayAccessible::TerminalDisplayAccessible(TerminalDisplay* display)
    : QAccessibleWidgetEx(display,
#if QT_VERSION > 0x040800 // added in Qt 4.8.1
                          QAccessible::Terminal
#else
                          QAccessible::EditableText
#endif
                         )
    , QAccessibleSimpleEditableTextInterface(this)
{}

int TerminalDisplayAccessible::characterCount()
{
    return display()->_usedLines * display()->_usedColumns;
}

int TerminalDisplayAccessible::cursorPosition()
{
    if (!display()->screenWindow())
        return 0;

    int offset = display()->_usedColumns * display()->screenWindow()->screen()->getCursorY();
    return offset + display()->screenWindow()->screen()->getCursorX();
}

void TerminalDisplayAccessible::selection(int selectionIndex, int* startOffset, int* endOffset)
{
    *startOffset = 0;
    *endOffset = 0;
    if (!display()->screenWindow() || selectionIndex)
        return;

    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    display()->screenWindow()->getSelectionStart(startColumn, startLine);
    display()->screenWindow()->getSelectionEnd(endColumn, endLine);
    if ((startLine == endLine) && (startColumn == endColumn))
        return;
    *startOffset = positionToOffset(startColumn, startLine);
    *endOffset = positionToOffset(endColumn, endLine);
}

int TerminalDisplayAccessible::selectionCount()
{
    if (!display()->screenWindow())
        return 0;

    int startLine;
    int startColumn;
    int endLine;
    int endColumn;
    display()->screenWindow()->getSelectionStart(startColumn, startLine);
    display()->screenWindow()->getSelectionEnd(endColumn, endLine);
    return ((startLine == endLine) && (startColumn == endColumn)) ? 0 : 1;
}

QString TerminalDisplayAccessible::visibleText() const
{
    // This function should be const to allow calling it from const interface functions.
    TerminalDisplay* display = const_cast<TerminalDisplayAccessible*>(this)->display();
    if (!display->screenWindow())
        return QString();

    return display->screenWindow()->screen()->text(0, display->_usedColumns * display->_usedLines, true);
}

void TerminalDisplayAccessible::addSelection(int startOffset, int endOffset)
{
    if (!display()->screenWindow())
        return;
    display()->screenWindow()->setSelectionStart(columnForOffset(startOffset), lineForOffset(startOffset), false);
    display()->screenWindow()->setSelectionEnd(columnForOffset(endOffset), lineForOffset(endOffset));
}

QString TerminalDisplayAccessible::attributes(int offset, int* startOffset, int* endOffset)
{
    // FIXME: this function should return css like attributes
    // as defined in the web ARIA standard
    Q_UNUSED(offset)
    *startOffset = 0;
    *endOffset = characterCount();
    return QString();
}

QRect TerminalDisplayAccessible::characterRect(int offset, QAccessible2::CoordinateType coordType)
{
    int row = offset / display()->_usedColumns;
    int col = offset - row * display()->_usedColumns;
    QPoint position = QPoint(col * display()->fontWidth() , row * display()->fontHeight());
    if(coordType == QAccessible2::RelativeToScreen)
        position = display()->mapToGlobal(position);
    return QRect(position, QSize(display()->fontWidth(), display()->fontHeight()));
}

int TerminalDisplayAccessible::offsetAtPoint(const QPoint& point, QAccessible2::CoordinateType coordType)
{
    // FIXME return the offset into the text from the given point
    Q_UNUSED(point)
    Q_UNUSED(coordType)
    return 0;
}

void TerminalDisplayAccessible::removeSelection(int selectionIndex)
{
    if (!display()->screenWindow() || selectionIndex)
        return;
    display()->screenWindow()->clearSelection();
}

void TerminalDisplayAccessible::scrollToSubstring(int startIndex, int endIndex)
{
    // FIXME: make sure the string between startIndex and endIndex is visible
    Q_UNUSED(startIndex)
    Q_UNUSED(endIndex)
}

void TerminalDisplayAccessible::setCursorPosition(int position)
{
    if (!display()->screenWindow())
        return;

    display()->screenWindow()->screen()->setCursorYX(lineForOffset(position), columnForOffset(position));
}

void TerminalDisplayAccessible::setSelection(int selectionIndex, int startOffset, int endOffset)
{
    if (selectionIndex)
        return;
    addSelection(startOffset, endOffset);
}

QString TerminalDisplayAccessible::text(QAccessible::Text t, int child) const
{
    if (t == QAccessible::Value && child == 0)
        return visibleText();
    return QAccessibleWidgetEx::text(t, child);
}

QString TerminalDisplayAccessible::text(int startOffset, int endOffset)
{
    if (!display()->screenWindow())
        return QString();

    return display()->screenWindow()->screen()->text(startOffset, endOffset, true);
}

QString TerminalDisplayAccessible::textAfterOffset(int offset, QAccessible2::BoundaryType boundaryType, int* startOffset, int* endOffset)
{
    const QString text = visibleText();
#if QT_VERSION >= 0x040800 // added in Qt 4.8.0
    return qTextAfterOffsetFromString(offset, boundaryType, startOffset, endOffset, text);
#else
    return text;
#endif
}

QString TerminalDisplayAccessible::textAtOffset(int offset, QAccessible2::BoundaryType boundaryType, int* startOffset, int* endOffset)
{
    const QString text = visibleText();
#if QT_VERSION >= 0x040800 // added in Qt 4.8.0
    return qTextAtOffsetFromString(offset, boundaryType, startOffset, endOffset, text);
#else
    return text;
#endif
}

QString TerminalDisplayAccessible::textBeforeOffset(int offset, QAccessible2::BoundaryType boundaryType, int* startOffset, int* endOffset)
{
    const QString text = visibleText();
#if QT_VERSION >= 0x040800 // added in Qt 4.8.0
    return qTextBeforeOffsetFromString(offset, boundaryType, startOffset, endOffset, text);
#else
    return text;
#endif
}

TerminalDisplay* TerminalDisplayAccessible::display()
{
    return static_cast<TerminalDisplay*>(object());
}
