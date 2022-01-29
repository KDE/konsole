/*
    SPDX-FileCopyrightText: 2022

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QPixmap>

struct TerminalGraphicsPlacement_t {
    QPixmap pixmap;
    qint64 id;
    qint64 pid;
    int z, X, Y, col, row, cols, rows;
    qreal opacity;
    bool scrolling;
};
