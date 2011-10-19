/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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
#include <QtCore/QList>
#include <QtGui/QMenu>

// KDE
#include <KLocale>

using namespace Konsole;

const TabTitleFormatAction::Element TabTitleFormatAction::_localElements[] =
{
    { "%n" , I18N_NOOP("Program Name: %n") },
    { "%d" , I18N_NOOP("Current Directory (Short): %d") },
    { "%D" , I18N_NOOP("Current Directory (Long): %D") },
    { "%w" , I18N_NOOP("Window Title Set by Shell: %w") },
    { "%#" , I18N_NOOP("Session Number: %#") },
    { "%u" , I18N_NOOP("User Name: %u") }
};
const int TabTitleFormatAction::_localElementCount =
    sizeof(_localElements) / sizeof(TabTitleFormatAction::Element);

const TabTitleFormatAction::Element TabTitleFormatAction::_remoteElements[] =
{
    { "%u" , I18N_NOOP("User Name: %u") },
    { "%h" , I18N_NOOP("Remote Host (Short): %h") },
    { "%H" , I18N_NOOP("Remote Host (Long): %H") },
    { "%w" , I18N_NOOP("Window Title Set by Shell: %w") },
    { "%#" , I18N_NOOP("Session Number: %#") }
};
const int TabTitleFormatAction::_remoteElementCount =
    sizeof(_remoteElements) / sizeof(TabTitleFormatAction::Element);

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

    QList<QAction*> actions;

    int count = 0;
    const Element* array = 0;

    if ( context == Session::LocalTabTitle )
    {
        array = _localElements;
        count = _localElementCount;
    }
    else if ( context == Session::RemoteTabTitle )
    {
        array = _remoteElements;
        count = _remoteElementCount;
    }

    for ( int i = 0 ; i < count ; i++ )
    {
        QAction* action = new QAction(i18n(array[i].description),this);
        action->setData(array[i].element);
        actions << action;
    }

    menu()->addActions(actions);
}
Session::TabTitleContext TabTitleFormatAction::context() const
{
    return _context;
}

#include "TabTitleFormatAction.moc"

