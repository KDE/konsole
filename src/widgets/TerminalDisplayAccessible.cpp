/*
 *  SPDX-FileCopyrightText: 2012 Frederik Gladhorn <gladhorn@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TerminalDisplayAccessible.h"
#include "session/SessionController.h"
#include <klocalizedstring.h>

#include "terminalDisplay/TerminalFonts.h"

using namespace Konsole;

TerminalDisplayAccessible::TerminalDisplayAccessible(TerminalDisplay *display)
    : QAccessibleWidget(display, QAccessible::Terminal, display->sessionController()->userTitle())
{
}

QString TerminalDisplayAccessible::text(QAccessible::Text t) const
{
    if (t == QAccessible::Value) {
        return visibleText();
    }
    return QAccessibleWidget::text(t);
}

int TerminalDisplayAccessible::characterCount() const
{
    return display()->_usedLines * display()->_usedColumns;
}

int TerminalDisplayAccessible::cursorPosition() const
{
    if (display()->screenWindow() == nullptr) {
        return 0;
    }

    int offset = display()->_usedColumns * display()->screenWindow()->screen()->getCursorY();
    return offset + display()->screenWindow()->screen()->getCursorX();
}

void TerminalDisplayAccessible::selection(int selectionIndex, int *startOffset, int *endOffset) const
{
    *startOffset = 0;
    *endOffset = 0;
    if ((display()->screenWindow() == nullptr) || (selectionIndex != 0)) {
        return;
    }

    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    display()->screenWindow()->getSelectionStart(startColumn, startLine);
    display()->screenWindow()->getSelectionEnd(endColumn, endLine);
    if ((startLine == endLine) && (startColumn == endColumn)) {
        return;
    }
    *startOffset = positionToOffset(startColumn, startLine);
    *endOffset = positionToOffset(endColumn, endLine);
}

int TerminalDisplayAccessible::selectionCount() const
{
    if (display()->screenWindow() == nullptr) {
        return 0;
    }

    int startLine = 0;
    int startColumn = 0;
    int endLine = 0;
    int endColumn = 0;
    display()->screenWindow()->getSelectionStart(startColumn, startLine);
    display()->screenWindow()->getSelectionEnd(endColumn, endLine);
    return ((startLine == endLine) && (startColumn == endColumn)) ? 0 : 1;
}

QString TerminalDisplayAccessible::visibleText() const
{
    // This function should be const to allow calling it from const interface functions.
    TerminalDisplay *display = const_cast<TerminalDisplayAccessible *>(this)->display();
    if (display->screenWindow() == nullptr) {
        return QString();
    }

    return display->screenWindow()->screen()->text(0, display->_usedColumns * display->_usedLines, Screen::PreserveLineBreaks);
}

void TerminalDisplayAccessible::addSelection(int startOffset, int endOffset)
{
    if (display()->screenWindow() == nullptr) {
        return;
    }
    display()->screenWindow()->setSelectionStart(columnForOffset(startOffset), lineForOffset(startOffset), false);
    display()->screenWindow()->setSelectionEnd(columnForOffset(endOffset), lineForOffset(endOffset), display()->trimTrailingWhitespace());
}

QString TerminalDisplayAccessible::attributes(int offset, int *startOffset, int *endOffset) const
{
    // FIXME: this function should return css like attributes
    // as defined in the web ARIA standard
    Q_UNUSED(offset)
    *startOffset = 0;
    *endOffset = characterCount();
    return QString();
}

QRect TerminalDisplayAccessible::characterRect(int offset) const
{
    int row = offset / display()->_usedColumns;
    int col = offset - row * display()->_usedColumns;
    QPoint position = QPoint(col * display()->terminalFont()->fontWidth(), row * display()->terminalFont()->fontHeight());
    return QRect(position, QSize(display()->terminalFont()->fontWidth(), display()->terminalFont()->fontHeight()));
}

int TerminalDisplayAccessible::offsetAtPoint(const QPoint &point) const
{
    // FIXME return the offset into the text from the given point
    Q_UNUSED(point)
    return 0;
}

void TerminalDisplayAccessible::removeSelection(int selectionIndex)
{
    if ((display()->screenWindow() == nullptr) || (selectionIndex != 0)) {
        return;
    }
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
    if (display()->screenWindow() == nullptr) {
        return;
    }

    display()->screenWindow()->screen()->setCursorYX(lineForOffset(position), columnForOffset(position));
}

void *TerminalDisplayAccessible::interface_cast(QAccessible::InterfaceType type)
{
    if (type == QAccessible::TextInterface) {
        return static_cast<QAccessibleTextInterface *>(this);
    }

    return QAccessibleWidget::interface_cast(type);
}

void TerminalDisplayAccessible::setSelection(int selectionIndex, int startOffset, int endOffset)
{
    if (selectionIndex != 0) {
        return;
    }
    addSelection(startOffset, endOffset);
}

QString TerminalDisplayAccessible::text(int startOffset, int endOffset) const
{
    if (display()->screenWindow() == nullptr) {
        return QString();
    }

    return display()->screenWindow()->screen()->text(startOffset, endOffset, Screen::PreserveLineBreaks);
}

TerminalDisplay *TerminalDisplayAccessible::display() const
{
    return qobject_cast<TerminalDisplay *>(widget());
}
