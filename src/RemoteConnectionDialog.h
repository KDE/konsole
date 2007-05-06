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

#ifndef REMOTECONNECTIONDIALOG_H
#define REMOTECONNECTIONDIALOG_H

// KDE
#include <KDialog>

namespace Ui
{
    class RemoteConnectionDialog;
}

namespace Konsole
{

class RemoteConnectionDialog : public KDialog
{
public:
    RemoteConnectionDialog(QWidget* parent = 0 );
    ~RemoteConnectionDialog();

    QString user() const;
    QString host() const;
    QString service() const;

    QString sessionKey() const;

private:
    Ui::RemoteConnectionDialog* _ui;
};

}

#endif // REMOTECONNECTIONDIALOG_H
