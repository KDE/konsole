/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef HOTSPOT
#define HOTSPOT

#include <QObject>

#include <QList>
#include <QRect>
#include <QRegion>
#include <QSharedPointer>

class QAction;
class QMenu;
class QMouseEvent;
class QKeyEvent;

namespace Konsole
{
class TerminalDisplay;

/**
 * Represents an area of text which matched the pattern a particular filter has been looking for.
 *
 * Each hotspot has a type identifier associated with it ( such as a link or a highlighted section ),
 * and an action.  When the user performs some activity such as a mouse-click in a hotspot area ( the exact
 * action will depend on what is displaying the block of text which the filter is processing ), the hotspot's
 * activate() method should be called.  Depending on the type of hotspot this will trigger a suitable response.
 *
 * For example, if a hotspot represents a URL then a suitable action would be opening that URL in a web browser.
 * Hotspots may have more than one action, in which case the list of actions can be obtained using the
 * actions() method.  These actions may then be displayed in a popup menu or toolbar for example.
 */
class HotSpot : public QObject
{
    // krazy suggest using Q_OBJECT here but moc can not handle
    // nested classes
    // QObject derived classes should use the Q_OBJECT macro

public:
    /**
     * Constructs a new hotspot which covers the area from (@p startLine,@p startColumn) to (@p endLine,@p endColumn)
     * in a block of text.
     */
    HotSpot(int startLine, int startColumn, int endLine, int endColumn);
    ~HotSpot() override;

    enum Type {
        // the type of the hotspot is not specified
        NotSpecified,
        // this hotspot rpresents a file on the filesystem
        File,
        // this hotspot represents a clickable URL link
        Link,
        // this hotspot represents a clickable e-mail address
        EMailAddress,
        // this hotspot represents a marker
        Marker,
        // this spot represents a Escaped URL
        EscapedUrl,
        // this hotspot represents a color of text
        Color,
    };

    /** Returns the line when the hotspot area starts */
    int startLine() const;
    /** Returns the line where the hotspot area ends */
    int endLine() const;
    /** Returns the column on startLine() where the hotspot area starts */
    int startColumn() const;
    /** Returns the column on endLine() where the hotspot area ends */
    int endColumn() const;
    /**
     * Returns the type of the hotspot.  This is usually used as a hint for views on how to represent
     * the hotspot graphically.  eg.  Link hotspots are typically underlined when the user mouses over them
     */
    Type type() const;
    /**
     * Causes the action associated with a hotspot to be triggered.
     *
     * @param object The object which caused the hotspot to be triggered.  This is
     * typically null ( in which case the default action should be performed ) or
     * one of the objects from the actions() list.  In which case the associated
     * action should be performed.
     */
    virtual void activate(QObject *object = nullptr) = 0;
    /**
     * Returns a list of actions associated with the hotspot which can be used in a
     * menu or toolbar
     */
    virtual QList<QAction *> actions();

    virtual bool hasDragOperation() const;
    virtual void startDrag();

    /**
     * Sets a menu up with actions for the hotspot.
     *
     * Returns a list of the added actions (useful for removing e.g.
     * the open-with actions before adding new ones to prevent duplicate
     * open-with actions being shown in @p menu).
     *
     * The base implementation does nothing.
     */
    virtual QList<QAction *> setupMenu(QMenu *menu);

    QPair<QRegion, QRect> region(int fontWidth, int fontHeight, int columns, QRect terminalDisplayRect) const;

    /**
     * Returns true if the type of the HotSpot is Link, EMailAddress, or EscapedUrl, (see
     * Type enum), otherwise returns false; mainly used in the input events, e.g. to not
     * change the shape of the mouse pointer to a pointing hand if the HotSpot doesn't
     * represent a clickable URI.
     */
    bool isUrl();

    /** The base implementation does nothing */
    virtual void mouseMoveEvent(TerminalDisplay *td, QMouseEvent *ev);

    /**
     * The mouse pointer shape is changed to a pointing hand; also because the underline
     * is painted under the link, update() is called on the TerminalDisplay widget.
     */
    virtual void mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev);

    /**
     * The mouse pointer is reset to the default shape, see TerminalDisplay::resetCursor();
     * also because the underline is hidden from under the link, update() is called on the
     * TerminalDisplay widget.
     */
    virtual void mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev);

    /**
     * If the Ctrl key is pressed or TerminalDisplay::openLinksByDirectClick() is
     * true, the activate() method is called to handle/open the link, see activate().
     */
    virtual void mouseReleaseEvent(TerminalDisplay *td, QMouseEvent *ev);

    /**
     * If the Ctrl key is pressed, the mouse pointer shape is changed to a pointing hand.
     *
     * Note that if TerminalDisplay::openLinksByDirectClick() is true the mouse pointer shape is always
     * changed to a pointing hand when hovering over a link, regardless of the state of the Ctrl key.
     */
    virtual void keyPressEvent(TerminalDisplay *td, QKeyEvent *ev);

    /**
     * This resets the mouse pointer to the default shape (if e.g. the Ctrl key had been pressed
     * and now has been released).
     *
     * Note that if TerminalDisplay::openLinksByDirectClick() is true the mouse pointer shape is always
     * changed to a pointing hand when hovering over a link, regardless of the state of the Ctrl key.
     */
    virtual void keyReleaseEvent(TerminalDisplay *, QKeyEvent *ev);

    void debug();

protected:
    /** Sets the type of a hotspot.  This should only be set once */
    void setType(Type type);

private:
    int _startLine;
    int _startColumn;
    int _endLine;
    int _endColumn;
    Type _type;
    QRegion _mouseOverHotspotArea;
};

}

#endif
