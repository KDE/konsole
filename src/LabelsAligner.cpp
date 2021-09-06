/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2018 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "LabelsAligner.h"

// Qt
#include <QGridLayout>
#include <QWidget>

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
    for (const auto *layout : qAsConst(_layouts)) {
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
    for (const auto *layout : qAsConst(_layouts)) {
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

    for (auto *l : qAsConst(_layouts)) {
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
