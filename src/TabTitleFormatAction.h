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

#ifndef TABTITLEFORMATACTION_H
#define TABTITLEFORMATACTION_H

// Qt
#include <QAction>

// Konsole
#include "Session.h"

namespace Konsole
{

class TabTitleFormatAction : public QAction
{
Q_OBJECT

public:
    TabTitleFormatAction(QObject* parent);
    ~TabTitleFormatAction();

    void setContext(Session::TabTitleContext context);
    Session::TabTitleContext context() const;

signals:
    void dynamicElementSelected(const QString&);
  
private slots:
    void fireElementSelected(QAction*);

private:
    Session::TabTitleContext _context;
    
    struct Element
    {
        QString element;
        char *description;
    };
    static const Element _localElements[];
    static const int _localElementCount;
    static const Element _remoteElements[];
    static const int _remoteElementCount;

};

}

#endif // TABTITLEFORMATACTION_H
