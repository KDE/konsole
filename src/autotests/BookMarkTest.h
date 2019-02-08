/*
    Copyright 2019 by Kurt Hindenburg <kurt.hindenburg@gmail.com>

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
