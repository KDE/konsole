/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWSPLITTER_H
#define VIEWSPLITTER_H

// Qt
#include <QSplitter>
#include <QSplitterHandle>

// Konsole
#include "konsoleprivate_export.h"

class QDragMoveEvent;
class QDragEnterEvent;
class QDropEvent;
class QDragLeaveEvent;

namespace Konsole
{
class TerminalDisplay;

class ViewSplitterHandle : public QSplitterHandle
{
    Q_OBJECT
public:
    ViewSplitterHandle(Qt::Orientation orientation, QSplitter *parent);

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseDoubleClickEvent(QMouseEvent *ev) override;

private:
    /* For some reason, the first time we double-click on the splitter handle
     * the second mouse press event is not fired, nor is the double click event.
     * We use this counter to detect a double click. */
    int mouseReleaseEventCounter;
};

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

    enum class AddBehavior {
        AddBefore,
        AddAfter,
    };
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
    void addTerminalDisplay(TerminalDisplay *terminalDisplay, Qt::Orientation containerOrientation, AddBehavior behavior = AddBehavior::AddAfter);

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

    /**
     * Can be called on any ViewSplitter to find the top level splitter and ensure
     * the active display isn't maximized. Do nothing if it's not maximized.
     *
     * Useful for ViewManager and TabbedViewContainer when removing or adding a
     * display to the hierarchy so that the layout is reflowed correctly.
     */
    void clearMaximized();

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

    void childEvent(QChildEvent *event) override;
    bool terminalMaximized() const
    {
        return m_terminalMaximized;
    }

    QSplitterHandle *createHandle() override;

    QPoint mapToTopLevel(const QPoint &p);
    QPoint mapFromTopLevel(const QPoint &p);

protected:
    void dragEnterEvent(QDragEnterEvent *ev) override;
    void dragMoveEvent(QDragMoveEvent *ev) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *ev) override;
    void showEvent(QShowEvent *) override;

Q_SIGNALS:
    void terminalDisplayDropped(TerminalDisplay *terminalDisplay);

private:
    /** recursively walks the object tree looking for Splitters and
     * TerminalDisplays, hiding the ones that should be hidden.
     * If a terminal display is not hidden in a subtree, we cannot
     * hide the whole tree.
     *
     * @p currentTerminalDisplay the only terminal display that will still be visible.
     */
    bool hideRecurse(TerminalDisplay *currentTerminalDisplay);

    /** other classes should use clearmMaximized() */
    void handleMinimizeMaximize(bool maximize);

    void updateSizes();
    bool m_terminalMaximized = false;
    bool m_blockPropagatedDeletion = false;

    static bool m_drawTopLevelHandler;
    static Qt::Orientation m_topLevelHandlerDrawnOrientation;
};
}
#endif // VIEWSPLITTER_H
