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

#ifndef EDITTABTITLEFORMATDIALOG_H
#define EDITTABTITLEFORMATDIALOG_H

// KDE
#include <KDialog>

// Konsole
#include "Session.h"

namespace Ui
{
    class EditTabTitleFormatDialog;
};

namespace Konsole
{

class EditTabTitleFormatDialog : public KDialog
{
Q_OBJECT
   
public:
    EditTabTitleFormatDialog(QWidget* parent = 0);
    virtual ~EditTabTitleFormatDialog();

    void setContext(Session::TabTitleContext context);

    void setTabTitleFormat(const QString& format);
    QString tabTitleFormat() const;

private slots:
    void insertElement(int index);

private:
    Ui::EditTabTitleFormatDialog* _ui;
    Session::TabTitleContext _context;

    struct Element
    {
        QString element;
        QString description;
    };
    static const Element _localElements[];
    static const int _localElementCount;
    static const Element _remoteElements[];
    static const int _remoteElementCount;
};

};

#endif // EDITTABTITLEFORMATDIALOG_H
