/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2018 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef LABELSALIGNER_H
#define LABELSALIGNER_H

#include <QObject>
#include <QVector>

class QWidget;
class QGridLayout;

namespace Konsole
{
/**
 * An utility class for aligning 0th column in multiple QGridLayouts.
 *
 * Limitations:
 * - a layout can't be nested in another layout
 * - reference widget must be an ancestor of all added layouts
 * - only 0th column is processed (widgets spanning multiple columns
 *   are ignored)
 */
class LabelsAligner : public QObject
{
    Q_OBJECT

public:
    explicit LabelsAligner(QWidget *refWidget);

    void addLayout(QGridLayout *layout);
    void addLayouts(const QVector<QGridLayout *> &layouts);
    void setReferenceWidget(QWidget *refWidget);

public Q_SLOTS:
    void updateLayouts();
    void align();

private:
    int getLeftMargin(const QGridLayout *layout);

    static constexpr int LABELS_COLUMN = 0;

    QWidget *_refWidget;
    QVector<QGridLayout *> _layouts;
};

}

#endif
