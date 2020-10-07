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

#ifndef LABELSALIGNER_H
#define LABELSALIGNER_H

#include <QObject>
#include <QVector>

class QWidget;
class QGridLayout;
template<typename T> class QVector;

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
    class LabelsAligner: public QObject {
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
