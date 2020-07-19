/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2020 by Tomaz Canabrava <tcanabrava@gmail.com>

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

#ifndef FILE_FILTER
#define FILE_FILTER

#include <QPointer>
#include <QString>
#include <QSet>

#include "RegExpFilter.h"

namespace Konsole {
class Session;
class HotSpot;

/**
 * A filter which matches files according to POSIX Portable Filename Character Set
 * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_267
 */

class FileFilter : public RegExpFilter
{
public:
    explicit FileFilter(Session *session);

    void process() override;

protected:
    QSharedPointer<HotSpot> newHotSpot(int, int, int, int, const QStringList &) override;

private:
    QPointer<Session> _session;
    QString _dirPath;
    QSet<QString> _currentDirContents;
};

}
#endif
