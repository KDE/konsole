/*
    This source file is part of Konsole, a terminal emulator.

    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef POPSTACKONEXIT_H
#define POPSTACKONEXIT_H

#include <QStack>

namespace Konsole
{
    /**
     * PopStackOnExit is a utility to remove all values from a QStack which are added during
     * the lifetime of a PopStackOnExit instance.
     *
     * When a PopStackOnExit instance is destroyed, elements are removed from the stack
     * until the stack count is reduced the value when the PopStackOnExit instance was created.
     */
    template<class T>
    class PopStackOnExit
    {
    public:
        explicit PopStackOnExit(QStack<T> &stack)
            : _stack(stack)
            , _count(stack.count())
        {
        }

        ~PopStackOnExit()
        {
            while (_stack.count() > _count) {
                _stack.pop();
            }
        }

    private:
        Q_DISABLE_COPY(PopStackOnExit)

        QStack<T> &_stack;
        int _count;
    };
}

#endif
