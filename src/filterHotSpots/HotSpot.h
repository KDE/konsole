/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2020 by Tomaz Canabrava <tcanabrava@gmail.com>

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

#ifndef HOTSPOT
#define HOTSPOT

#include <QObject>

#include <QList>

class QAction;
class QMenu;

namespace Konsole {
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
    virtual ~HotSpot();

    enum Type {
        // the type of the hotspot is not specified
        NotSpecified,
        // this hotspot represents a clickable link
        Link,
        // this hotspot represents a clickable e-mail address
        EMailAddress,
        // this hotspot represents a marker
        Marker,
        // this spot represents a Escaped URL
        EscapedUrl,
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

    /**
        * Setups a menu with actions for the hotspot.
        */
    virtual void setupMenu(QMenu *menu);
protected:
    /** Sets the type of a hotspot.  This should only be set once */
    void setType(Type type);

private:
    int _startLine;
    int _startColumn;
    int _endLine;
    int _endColumn;
    Type _type;
};

}

#endif
