/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2018 by Harald Sitter <sitter@kde.org>

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

// Own
#include "LabelsAligner.h"

// Qt
#include <QWidget>
#include <QGridLayout>

using namespace Konsole;

LabelsAligner::LabelsAligner(QWidget *refWidget)
    : _refWidget(refWidget)
{
}

void LabelsAligner::addLayout(QGridLayout *layout)
{
    _layouts.append(layout);
}


void LabelsAligner::addLayouts(const QVector<QGridLayout *> &layouts)
{
    _layouts.append(layouts);
}

void LabelsAligner::setReferenceWidget(QWidget *refWidget)
{
    _refWidget = refWidget;
}

void LabelsAligner::updateLayouts()
{
    for (const auto *layout: qAsConst(_layouts)) {
        QWidget *widget = layout->parentWidget();
        Q_ASSERT(widget);
        do {
            QLayout *widgetLayout = widget->layout();
            if (widgetLayout != nullptr) {
                widgetLayout->update();
                widgetLayout->activate();
            }
            widget = widget->parentWidget();
        } while (widget != _refWidget && widget != nullptr);
    }
}

void LabelsAligner::align()
{
    Q_ASSERT(_refWidget);

    if (_layouts.count() <= 1) {
        return;
    }

    int maxRight = 0;
    for (const auto *layout: qAsConst(_layouts)) {
        int left = getLeftMargin(layout);
        for (int row = 0; row < layout->rowCount(); ++row) {
            QLayoutItem *layoutItem = layout->itemAtPosition(row, LABELS_COLUMN);
            if (layoutItem == nullptr) {
                continue;
            }
            QWidget *widget = layoutItem->widget();
            if (widget == nullptr) {
                continue;
            }
            const int idx = layout->indexOf(widget);
            int rows, cols, rowSpan, colSpan;
            layout->getItemPosition(idx, &rows, &cols, &rowSpan, &colSpan);
            if (colSpan > 1) {
                continue;
            }

            const int right = left + widget->sizeHint().width();
            if (maxRight < right) {
                maxRight = right;
            }
        }
    }

    for (auto *l: qAsConst(_layouts)) {
        int left = getLeftMargin(l);
        l->setColumnMinimumWidth(LABELS_COLUMN, maxRight - left);
    }
}

int LabelsAligner::getLeftMargin(const QGridLayout *layout)
{
    int left = layout->contentsMargins().left();

    if (layout->parent()->isWidgetType()) {
        auto *parentWidget = layout->parentWidget();
        Q_ASSERT(parentWidget);
        left += parentWidget->contentsMargins().left();
    } else {
        auto *parentLayout = qobject_cast<QLayout *>(layout->parent());
        Q_ASSERT(parentLayout);
        left += parentLayout->contentsMargins().left();
    }

    QWidget *parent = layout->parentWidget();
    while (parent != _refWidget && parent != nullptr) {
        left = parent->mapToParent(QPoint(left, 0)).x();
        parent = parent->parentWidget();
    }
    return left;
}
