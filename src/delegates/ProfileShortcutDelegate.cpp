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

#include "ProfileShortcutDelegate.h"

#include <QApplication>
#include <QKeyEvent>
#include <QPainter>

using namespace Konsole;

ShortcutItemDelegate::ShortcutItemDelegate(QObject *parent)
    : QStyledItemDelegate(parent),
    _modifiedEditors(QSet<QWidget*>()),
    _itemsBeingEdited(QSet<QModelIndex>())
{
}

void ShortcutItemDelegate::editorModified()
{
    auto *editor = qobject_cast<FilteredKeySequenceEdit*>(sender());
    Q_ASSERT(editor);
    _modifiedEditors.insert(editor);
    emit commitData(editor);
    emit closeEditor(editor);
}
void ShortcutItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                        const QModelIndex &index) const
{
    _itemsBeingEdited.remove(index);

    if (!_modifiedEditors.contains(editor)) {
        return;
    }

    const auto shortcut = qobject_cast<FilteredKeySequenceEdit *>(editor)->keySequence().toString();
    model->setData(index, shortcut, Qt::DisplayRole);

    _modifiedEditors.remove(editor);
}

QWidget* ShortcutItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem&, const QModelIndex &index) const
{
    _itemsBeingEdited.insert(index);

    auto editor = new FilteredKeySequenceEdit(parent);
    const auto shortcutString = index.data(Qt::DisplayRole).toString();

    editor->setKeySequence(QKeySequence::fromString(shortcutString));
    editor->setFocus(Qt::FocusReason::MouseFocusReason);

    connect(editor, &QKeySequenceEdit::editingFinished, this, &Konsole::ShortcutItemDelegate::editorModified);

    return editor;
}
void ShortcutItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    if (_itemsBeingEdited.contains(index)) {
        StyledBackgroundPainter::drawBackground(painter, option, index);
    } else {
        QStyledItemDelegate::paint(painter, option, index);
    }
}

QSize ShortcutItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto shortcutString = index.data(Qt::DisplayRole).toString();
    const auto fm = option.fontMetrics;

    static const int editorMargins = 16; // chosen empirically
    const int width = fm.boundingRect(shortcutString + QStringLiteral(", ...")).width()
                      + editorMargins;

    return {width, QStyledItemDelegate::sizeHint(option, index).height()};
}

void ShortcutItemDelegate::destroyEditor(QWidget *editor, const QModelIndex &index) const
{
    _itemsBeingEdited.remove(index);
    _modifiedEditors.remove(editor);
    editor->deleteLater();
}


void FilteredKeySequenceEdit::keyPressEvent(QKeyEvent *event)
{
    if(event->modifiers() == Qt::NoModifier) {
        switch(event->key()) {
        case Qt::Key_Enter:
        case Qt::Key_Return:
            emit editingFinished();
            return;
        case Qt::Key_Backspace:
        case Qt::Key_Delete:
            clear();
            emit editingFinished();
            event->accept();
            return;
        default:
            event->accept();
            return;
        }
    }
    QKeySequenceEdit::keyPressEvent(event);
}

void StyledBackgroundPainter::drawBackground(QPainter *painter, const QStyleOptionViewItem &option,
        const QModelIndex&)
{
    const auto* opt = qstyleoption_cast<const QStyleOptionViewItem*>(&option);
    const QWidget* widget = opt != nullptr ? opt->widget : nullptr;

    QStyle* style = widget != nullptr ? widget->style() : QApplication::style();

    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, widget);
}

