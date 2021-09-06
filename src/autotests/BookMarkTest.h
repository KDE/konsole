/*
    SPDX-FileCopyrightText: 2019 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef BOOKMARKTEST_H
#define BOOKMARKTEST_H

#include <QObject>

namespace Konsole
{
class BookMarkTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testBookMarkURLs_data();
    void testBookMarkURLs();

private:
};

}

#endif // BOOKMARKTEST_H
