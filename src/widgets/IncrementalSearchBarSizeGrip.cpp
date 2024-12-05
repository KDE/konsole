/*
    SPDX-FileCopyrightText: 2024 Eric D'Addario <ericdaddario02@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "widgets/IncrementalSearchBarSizeGrip.h"

namespace Konsole
{
IncrementalSearchBarSizeGrip::IncrementalSearchBarSizeGrip(QWidget *parent)
    : QSizeGrip(parent)
{
    this->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Ignored);
    this->setFixedWidth(5);
}

void IncrementalSearchBarSizeGrip::moveEvent(QMoveEvent *moveEvent)
{
    QSizeGrip::moveEvent(moveEvent);
    setCursor(Qt::SizeHorCursor);
}
}