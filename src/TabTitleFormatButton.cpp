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
#include "TabTitleFormatButton.h"

// Qt
#include <QtCore/QList>
#include <QMenu>

// KDE
#include <KLocalizedString>

using namespace Konsole;

const TabTitleFormatButton::Element TabTitleFormatButton::_localElements[] = {
    { "%n" , I18N_NOOP("Program Name: %n") },
    { "%d" , I18N_NOOP("Current Directory (Short): %d") },
    { "%D" , I18N_NOOP("Current Directory (Long): %D") },
    { "%w" , I18N_NOOP("Window Title Set by Shell: %w") },
    { "%#" , I18N_NOOP("Session Number: %#") },
    { "%u" , I18N_NOOP("User Name: %u") },
    { "%h" , I18N_NOOP("Local Host: %h") }
};
const int TabTitleFormatButton::_localElementCount =
    sizeof(_localElements) / sizeof(TabTitleFormatButton::Element);

const TabTitleFormatButton::Element TabTitleFormatButton::_remoteElements[] = {
    { "%u" , I18N_NOOP("User Name: %u") },
    { "%h" , I18N_NOOP("Remote Host (Short): %h") },
    { "%H" , I18N_NOOP("Remote Host (Long): %H") },
    { "%w" , I18N_NOOP("Window Title Set by Shell: %w") },
    { "%#" , I18N_NOOP("Session Number: %#") }
};
const int TabTitleFormatButton::_remoteElementCount =
    sizeof(_remoteElements) / sizeof(TabTitleFormatButton::Element);

TabTitleFormatButton::TabTitleFormatButton(QWidget* aParent)
    : QPushButton(aParent)
    , _context(Session::LocalTabTitle)
{
    setText(i18n("Insert"));
    setMenu(new QMenu());
    connect(menu() , &QMenu::triggered , this , &Konsole::TabTitleFormatButton::fireElementSelected);
}

TabTitleFormatButton::~TabTitleFormatButton()
{
    menu()->deleteLater();
}

void TabTitleFormatButton::fireElementSelected(QAction* action)
{
    emit dynamicElementSelected(action->data().value<QString>());
}

void TabTitleFormatButton::setContext(Session::TabTitleContext titleContext)
{
    _context = titleContext;

    menu()->clear();

    int count = 0;
    const Element* array = 0;

    if (titleContext == Session::LocalTabTitle) {
        setToolTip(i18nc("@info:tooltip", "Insert title format"));
        array = _localElements;
        count = _localElementCount;
    } else if (titleContext == Session::RemoteTabTitle) {
        setToolTip(i18nc("@info:tooltip", "Insert remote title format"));
        array = _remoteElements;
        count = _remoteElementCount;
    }

    QList<QAction*> menuActions;
    for (int i = 0 ; i < count ; i++) {
        QAction* action = new QAction(i18n(array[i].description), this);
        action->setData(array[i].element);
        menuActions << action;
    }

    menu()->addActions(menuActions);
}

Session::TabTitleContext TabTitleFormatButton::context() const
{
    return _context;
}

