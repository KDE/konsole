/*
    This file is part of the Konsole Terminal.

    Copyright 2006-2008 Robert Knight <robertknight@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

#ifndef VIEWSPLITTER_H
#define VIEWSPLITTER_H

// Qt
#include <QSplitter>

// Konsole
#include "konsoleprivate_export.h"

class QDragMoveEvent;
class QDragEnterEvent;
class QDropEvent;
class QDragLeaveEvent;

namespace Konsole {
class TerminalDisplay;

/**
 * A splitter which holds a number of ViewContainer objects and allows
 * the user to control the size of each view container by dragging a splitter
 * bar between them.
 *
 * Each splitter can also contain child ViewSplitter widgets, allowing
 * for a hierarchy of view splitters and containers.
 *
 * The addContainer() method is used to split the existing view and
 * insert a new view container.
 * Containers can only be removed from the hierarchy by deleting them.
 */
class KONSOLEPRIVATE_EXPORT ViewSplitter : public QSplitter
{
    Q_OBJECT

public:
    explicit ViewSplitter(QWidget *parent = nullptr);
    enum class AddBehavior {AddBefore, AddAfter};
    /**
     * Locates the child ViewSplitter widget which currently has the focus
     * and inserts the container into it.
     *
     * @param terminalDisplay The container to insert
     * @param containerOrientation Specifies whether the view should be split
     *                    horizontally or vertically.  If the orientation
     *                    is the same as the ViewSplitter into which the
     *                    container is to be inserted, or if the splitter
     *                    has fewer than two child widgets then the container
     *                    will be added to that splitter.  If the orientation
     *                    is different, then a new child splitter
     *                    will be created, into which the container will
     *                    be inserted.
     * @param behavior Specifies whether to add new terminal after current
     *                 tab or at end.
     */
    void addTerminalDisplay(TerminalDisplay* terminalDisplay, Qt::Orientation containerOrientation, AddBehavior behavior = AddBehavior::AddAfter);

    /** Returns the child ViewSplitter widget which currently has the focus */
    ViewSplitter *activeSplitter();

    /**
     * Returns the container which currently has the focus or 0 if none
     * of the immediate child containers have the focus.  This does not
     * search through child splitters.  activeSplitter() can be used
     * to search recursively through child splitters for the splitter
     * which currently has the focus.
     *
     * To find the currently active container, use
     * mySplitter->activeSplitter()->activeTerminalDisplay() where
     * mySplitter is the ViewSplitter widget at the top of the hierarchy.
     */
    TerminalDisplay *activeTerminalDisplay() const;

    /** Makes the current TerminalDisplay expanded to 100% of the view
     */
    void toggleMaximizeCurrentTerminal();

    void handleMinimizeMaximize(bool maximize);

    /** returns the splitter that has no splitter as a parent. */
    ViewSplitter *getToplevelSplitter();

    /**
     * Changes the size of the specified @p container by a given @p percentage.
     * @p percentage may be positive ( in which case the size of the container
     * is increased ) or negative ( in which case the size of the container
     * is decreased ).
     *
     * The sizes of the remaining containers are increased or decreased
     * uniformly to maintain the width of the splitter.
     */
    void adjustActiveTerminalDisplaySize(int percentage);

    void focusUp();
    void focusDown();
    void focusLeft();
    void focusRight();

    void handleFocusDirection(Qt::Orientation orientation, int direction);

    void childEvent(QChildEvent* event) override;
    bool terminalMaximized() const { return m_terminalMaximized; }

protected:
    void dragEnterEvent(QDragEnterEvent *ev) override;
    void dragMoveEvent(QDragMoveEvent *ev) override;
    void dragLeaveEvent(QDragLeaveEvent * event) override;
    void dropEvent(QDropEvent *ev) override;
    void showEvent(QShowEvent *) override;

Q_SIGNALS:
    void terminalDisplayDropped(TerminalDisplay *terminalDisplay);

private:
    /** recursively walks the object tree looking for Splitters and
     * TerminalDisplays, hidding the ones that should be hidden.
     * If a terminal display is not hidden in a subtree, we cannot
     * hide the whole tree.
     *
     * @p currentTerminalDisplay the only terminal display that will still be visible.
     */
    bool hideRecurse(TerminalDisplay *currentTerminalDisplay);

    void updateSizes();
    bool m_terminalMaximized = false;
};
}
#endif //VIEWSPLITTER_H
