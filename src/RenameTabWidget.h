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

#ifndef RENAMETABWIDGET_H
#define RENAMETABWIDGET_H

// Qt
#include <QWidget>

namespace Ui
{
class RenameTabWidget;
}

namespace Konsole
{
class RenameTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RenameTabWidget(QWidget* parent = 0);
    ~RenameTabWidget();

    QString tabTitleText() const;
    QString remoteTabTitleText() const;
    void setTabTitleText(const QString&);
    void setRemoteTabTitleText(const QString&);

    void focusTabTitleText();
    void focusRemoteTabTitleText();

signals:
    void tabTitleFormatChanged(const QString&);
    void remoteTabTitleFormatChanged(const QString&);

public slots:
    void insertTabTitleText(const QString& text);
    void insertRemoteTabTitleText(const QString& text);

private:
    Ui::RenameTabWidget* _ui;
};
}

#endif
