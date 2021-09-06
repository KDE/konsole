/*
    This source file is part of Konsole, a terminal emulator.

    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
