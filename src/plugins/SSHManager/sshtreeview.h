/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHTREEVIEW_H
#define SSHTREEVIEW_H

#include <QTreeView>

class SshTreeView : public QTreeView
{
    Q_OBJECT
public:
    SshTreeView(QWidget *parent = nullptr);
    ~SshTreeView() noexcept override;

    // mouseClicked already exists but only sends QModelIndex
    Q_SIGNAL void mouseButtonClicked(Qt::MouseButton button, const QModelIndex &idx);
    void mouseReleaseEvent(QMouseEvent *ev) override;
};

#endif
