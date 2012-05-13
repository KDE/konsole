/*
    Copyright 2010 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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

#ifndef RENAMETABDIALOG_H
#define RENAMETABDIALOG_H

// KDE
#include <KDialog>

namespace Ui
{
class RenameTabDialog;
}

namespace Konsole
{
class RenameTabDialog : public KDialog
{
    Q_OBJECT

public:
    explicit RenameTabDialog(QWidget* parent = 0);
    ~RenameTabDialog();

    QString tabTitleText() const;
    QString remoteTabTitleText() const;
    void setTabTitleText(const QString&);
    void setRemoteTabTitleText(const QString&);

    void focusTabTitleText();
    void focusRemoteTabTitleText();

private:
    Ui::RenameTabDialog* _ui;
};
}

#endif
