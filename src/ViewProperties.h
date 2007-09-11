/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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

#ifndef VIEWPROPERTIES_H
#define VIEWPROPERTIES_H

// Qt
#include <QtGui/QIcon>
#include <QtCore/QObject>

// KDE
#include <KUrl>

namespace Konsole
{

/** 
 * Encapsulates user-visible information about the terminal session currently being displayed in a view,
 * such as the associated title and icon.
 *
 * This can be used by navigation widgets in a ViewContainer sub-class to provide a tab, label or other
 * item for switching between views.
 */
class ViewProperties : public QObject 
{
Q_OBJECT

public:
    ViewProperties(QObject* parent); 

    /** Returns the icon associated with a view */
    QIcon icon() const;
    /** Returns the title associated with a view */
    QString title() const;

    /** 
     * Returns the URL current associated with a view.
     * The default implementation returns an empty URL. 
     */
    virtual KUrl url() const;

    /**
     * Returns the current directory associated with a view.
     * This may be the same as url()
     *
     * The default implementation returns an empty string.
     */
    virtual QString currentDir() const;

    /** 
     * A unique identifier representing the data displayed by the view associated with this
     * ViewProperties instance.
     *
     * TODO: Finish implementation and documentation 
     *
     * This can be used when dragging and dropping views between windows so that [ FINISH ME ]
     */
    int identifier() const;

signals:
    /** Emitted when the icon for a view changes */
    void iconChanged(ViewProperties* properties);
    /** Emitted when the title for a view changes */
    void titleChanged(ViewProperties* properties);
    /** Emitted when activity has occurred in this view. */
    void activity(ViewProperties* item);

public slots:
    /**
     * Requests the renaming of this view.
     * 
     * The default implementation does nothing.
     */
     virtual void rename();

protected slots:
    /**
     * Emits the activity() signal.
     */
    void fireActivity();

protected:
    /** 
     * Subclasses may call this method to change the title.  This causes
     * a titleChanged() signal to be emitted 
     */
    void setTitle(const QString& title);
    /**
     * Subclasses may call this method to change the icon.  This causes
     * an iconChanged() signal to be emitted
     */
    void setIcon(const QIcon& icon);
    /**
     * Subclasses may call this method to change the identifier.  
     */
    void setIdentifier(int id);

    
private:
    QIcon _icon;
    QString _title;
    int _id;
};

}

#endif //VIEWPROPERTIES_H
