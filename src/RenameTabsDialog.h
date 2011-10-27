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

#ifndef RENAMETABSDIALOG
#define RENAMETABSDIALOG

// KDE
#include <KDialog>

namespace Ui
{
    class RenameTabsDialog;
}

namespace Konsole
{

class RenameTabsDialog : public KDialog
{
Q_OBJECT

public:
    RenameTabsDialog(QWidget* parent = 0);
    ~RenameTabsDialog();
    QString tabTitleText() const;
    QString remoteTabTitleText() const;
    void setTabTitleText(const QString&);
    void setRemoteTabTitleText(const QString&);

    void focusTabTitleText();
    void focusRemoteTabTitleText();

public slots:
    void insertTabTitleText(const QString& text);
    void insertRemoteTabTitleText(const QString& text);

private:
    Ui::RenameTabsDialog* _ui;
};

}

#endif
