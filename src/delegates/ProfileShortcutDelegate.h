/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PROFILESHORTCUTDELEGATE_H
#define PROFILESHORTCUTDELEGATE_H

#include <QKeySequenceEdit>
#include <QModelIndex>
#include <QSet>
#include <QStyledItemDelegate>

class QWidget;
class QKeyEvent;
class QPainter;

namespace Konsole
{
class StyledBackgroundPainter
{
public:
    static void drawBackground(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index);
};

class FilteredKeySequenceEdit : public QKeySequenceEdit
{
    Q_OBJECT

public:
    explicit FilteredKeySequenceEdit(QWidget *parent = nullptr)
        : QKeySequenceEdit(parent)
    {
    }

protected:
    void keyPressEvent(QKeyEvent *event) override;
};

class ShortcutItemDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit ShortcutItemDelegate(QObject *parent = nullptr);

    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const override;
    QWidget *createEditor(QWidget *aParent, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void destroyEditor(QWidget *editor, const QModelIndex &index) const override;

private Q_SLOTS:
    void editorModified();

private:
    mutable QSet<QWidget *> _modifiedEditors;
    mutable QSet<QModelIndex> _itemsBeingEdited;
};

}
#endif
