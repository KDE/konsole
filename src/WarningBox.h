/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

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

#ifndef WARNINGBOX_H
#define WARNINGBOX_H

// Qt
#include <QtGui/QFrame>

class QLabel;

namespace Konsole
{

/**
 * A label which displays a warning message,
 * using the appropriate icon from the current icon theme
 * and background color from KColorScheme
 */
class WarningBox : public QFrame
{
    Q_OBJECT

public:
    WarningBox(QWidget* parent = 0);

    /** Sets the text displayed in the warning label. */
    void setText(const QString& text);
    /** Returns the text displayed in the warning label. */
    QString text() const;

private:
    QLabel* _label;
};

}

#endif // WARNINGBOX_H


