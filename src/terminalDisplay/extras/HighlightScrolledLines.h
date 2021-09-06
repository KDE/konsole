/*
    SPDX-FileCopyrightText: 2020-2020 Carlos Alves <cbcalves@gmail.com>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HIGHLIGHTSCROLLEDLINES_HPP
#define HIGHLIGHTSCROLLEDLINES_HPP

// Qt
#include <QRect>
#include <QTimer>
#include <QWidget>

// Konsole
#include "Enumeration.h"
#include "konsoleprivate_export.h"

namespace Konsole
{
class TerminalScrollBar;
/**
 * Control the highlight the lines that are coming into view.
 * A thin blue line on the left of the terminal that highlight
 * the new lines in the following situations:
 * - scrolling with the mouse
 * - using the scroll bar
 * - using the keyboard to move up/down
 * - new lines resulting from the output of a command
 */
class HighlightScrolledLines
{
public:
    HighlightScrolledLines();
    ~HighlightScrolledLines();
    /**
     * Return if highlight lines is enabled
     */
    bool isEnabled();
    /**
     * Enable or disable the highlight lines
     */
    void setEnabled(bool enable);
    /**
     * Return if the highlight line needs to clear
     */
    bool isNeedToClear();
    /**
     * Set the highlight line needs to clear
     */
    void setNeedToClear(bool isNeeded);
    int getPreviousScrollCount();
    void setPreviousScrollCount(int scrollCount);
    /**
     * Set the highlight line timer if it's enabled
     */
    void setTimer(TerminalScrollBar *parent);
    /**
     * Starts the highlight line timer
     */
    void startTimer();
    /**
     * Return if the highlight line timer is active
     */
    bool isTimerActive();
    /**
     * Highlight line size of the blue line
     */
    QRect &rect();
    static const int HIGHLIGHT_SCROLLED_LINES_WIDTH = 3;

private:
    bool _enabled;
    QRect _rect;
    int _previousScrollCount;
    std::unique_ptr<QTimer> _timer;
    bool _needToClear;
};
} // namespace Konsole

#endif // HIGHLIGHTSCROLLEDLINES_HPP
