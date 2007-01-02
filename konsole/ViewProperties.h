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
#include <QIcon>
#include <QObject>


/** 
 * Provides access to information such as the title and icon associated with a document
 * in a view container 
 */
class ViewProperties : public QObject 
{
Q_OBJECT

public:
    ViewProperties(QObject* parent) : QObject(parent) {}

    /** Returns the icon associated with a view */
    QIcon icon();
    /** Returns the title associated with a view */
    QString title();

signals:
    /** Emitted when the icon for a view changes */
    void iconChanged(ViewProperties* properties);
    /** Emitted when the title for a view changes */
    void titleChanged(ViewProperties* properties);

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

private:
    QIcon _icon;
    QString _title;
};

#endif //VIEWPROPERTIES_H
