/*
    SPDX-FileCopyrightText: 2024 Eric D'Addario <ericdaddario02@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef INCREMENTALSEARCHBARSIZEGRIP_H
#define INCREMENTALSEARCHBARSIZEGRIP_H

// Qt
#include <QSizeGrip>

namespace Konsole
{
/**
 * A custom QSizeGrip widget used by IncrementalSearchBar which allows using a QSizeGrip
 * with a horizontal resize cursor.
 *
 * Currently, QSizeGrip only uses diagonal resize cursors, so we must override this
 * functionality as a workaround.
 */
class IncrementalSearchBarSizeGrip : public QSizeGrip
{
public:
    explicit IncrementalSearchBarSizeGrip(QWidget *parent = nullptr);

protected:
    /**
     * QSizeGrip sets the cursor inside moveEvent().
     * We override this function to set our desired cursor.
     */
    void moveEvent(QMoveEvent *moveEvent) override;
};
}

#endif
