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

// Own
#include "TabTitleFormatAction.h"

// Qt
#include <QList>
#include <QMenu>

// KDE
#include <KLocale>

using namespace Konsole;

const TabTitleFormatAction::Element TabTitleFormatAction::_localElements[] = 
{
    { "%n" , i18n("Program Name") },
    { "%p" , i18n("Profile Name") },
    { "%d" , i18n("Current Directory (Short)") },
    { "%D" , i18n("Current Directory (Long)") }
};
const int TabTitleFormatAction::_localElementCount = 4;
const TabTitleFormatAction::Element TabTitleFormatAction::_remoteElements[] =
{
    { "%p" , i18n("Profile Name") },
    { "%u" , i18n("User Name") },
    { "%h" , i18n("Remote Host (Short)") },
    { "%H" , i18n("Remote Host (Long)") }
};
const int TabTitleFormatAction::_remoteElementCount = 4;

TabTitleFormatAction::TabTitleFormatAction(QObject* parent)
    : QAction(parent)
    , _context(Session::LocalTabTitle)
{
    setMenu( new QMenu() );
    connect( menu() , SIGNAL(triggered(QAction*)) , this , SLOT(fireElementSelected(QAction*)) );
}
TabTitleFormatAction::~TabTitleFormatAction()
{
    menu()->deleteLater();
}
void TabTitleFormatAction::fireElementSelected(QAction* action)
{
    emit dynamicElementSelected(action->data().value<QString>());
}
void TabTitleFormatAction::setContext(Session::TabTitleContext context)
{
    _context = context;

    menu()->clear();

    QList<QAction*> list;
    
    int count = 0;
    const Element* array = 0;

    if ( context == Session::LocalTabTitle )
    {
        count = _localElementCount;
        array = _localElements;    
    }
    else if ( context == Session::RemoteTabTitle )
    {
        count = _remoteElementCount;
        array = _remoteElements;
    }
     
    for ( int i = 0 ; i < count ; i++ )
    {
        QAction* action = new QAction(array[i].description,this);
        action->setData(array[i].element);
        list << action;
    }

    menu()->addActions(list);
}
Session::TabTitleContext TabTitleFormatAction::context() const
{
    return _context;
}

#include "TabTitleFormatAction.moc"

