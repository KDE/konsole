/*
 *  SPDX-FileCopyrightText: 2012 Frederik Gladhorn <gladhorn@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TERMINALDISPLAYACCESSIBLE_H
#define TERMINALDISPLAYACCESSIBLE_H

//#include <QtGlobal>

#include <QAccessibleTextInterface>
#include <QAccessibleWidget>

#include "Screen.h"
#include "ScreenWindow.h"
#include "terminalDisplay/TerminalDisplay.h"

namespace Konsole
{
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
