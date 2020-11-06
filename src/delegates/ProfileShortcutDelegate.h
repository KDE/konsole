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

#ifndef PROFILESHORTCUTDELEGATE_H
#define PROFILESHORTCUTDELEGATE_H

#include <QStyledItemDelegate>
#include <QModelIndex>
#include <QSet>
#include <QKeySequenceEdit>

class QWidget;
class QKeyEvent;
class QPainter;

namespace Konsole
{

class StyledBackgroundPainter
{
public:
    static void drawBackground(QPainter *painter, const QStyleOptionViewItem &option,
                               const QModelIndex &index);
};

class FilteredKeySequenceEdit: public QKeySequenceEdit
{
    Q_OBJECT

public:
    explicit FilteredKeySequenceEdit(QWidget *parent = nullptr): QKeySequenceEdit(parent) {}

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

class ShortcutItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ShortcutItemDelegate(QObject *parent = nullptr);

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *aParent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    void destroyEditor(QWidget *editor, const QModelIndex &index) const override;

private Q_SLOTS:
    void editorModified();

private:
    mutable QSet<QWidget *> _modifiedEditors;
    mutable QSet<QModelIndex> _itemsBeingEdited;
};

}
#endif
