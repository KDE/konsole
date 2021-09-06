/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2018 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef COLORSCHEMEVIEWDELEGATE_H
#define COLORSCHEMEVIEWDELEGATE_H

#include <QAbstractItemDelegate>

namespace Konsole
{
/**
 * A delegate which can display and edit color schemes in a view.
 */
class ColorSchemeViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT

public:
    explicit ColorSchemeViewDelegate(QObject *parent = nullptr);

    // reimplemented
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

}

#endif
