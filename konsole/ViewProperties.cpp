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

#include "ViewProperties.h"

void ViewProperties::setTitle(const QString& title)
{
    if ( title != _title )
    {
        _title = title;
        emit titleChanged(this);
    }
}
void ViewProperties::setIcon(const QIcon& icon)
{
    _icon = icon;
    emit iconChanged(this);
}
QString ViewProperties::title()
{
    return _title;
}
QIcon ViewProperties::icon()
{
    return _icon;
}

#include "ViewProperties.moc"
