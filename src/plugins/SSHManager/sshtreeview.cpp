/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshtreeview.h"

#include <QMouseEvent>

SshTreeView::SshTreeView(QWidget *parent)
    : QTreeView(parent)
{
}

SshTreeView::~SshTreeView() noexcept = default;

void SshTreeView::mouseReleaseEvent(QMouseEvent *ev)
{
    const QModelIndex idxAt = indexAt(ev->pos());
    if (idxAt.isValid()) {
        Q_EMIT mouseButtonClicked(ev->button(), idxAt);
    }
}
