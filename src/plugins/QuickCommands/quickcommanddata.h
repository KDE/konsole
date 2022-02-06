// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUICKCOMMANDDATA_H
#define QUICKCOMMANDDATA_H

#include <QString>

class QuickCommandData
{
public:
    QString name;
    QString tooltip;
    QString command;
};

Q_DECLARE_METATYPE(QuickCommandData)

#endif // QUICKCOMMANDDATA_H
