/*
    This file is part of Konsole, an X terminal.
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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

#ifndef HISTORYFILE_H
#define HISTORYFILE_H

// Qt
#include <QTemporaryFile>

#include "konsoleprivate_export.h"

namespace Konsole
{

/*
   An extendable tmpfile(1) based buffer.
*/
class HistoryFile
{
public:
    HistoryFile();
    virtual ~HistoryFile();

    virtual void add(const char *buffer, qint64 count);
    virtual void get(char *buffer, qint64 size, qint64 loc);
    virtual qint64 len() const;

    //mmaps the file in read-only mode
    void map();
    //un-mmaps the file
    void unmap();

private:
    qint64 _length;
    QTemporaryFile _tmpFile;

    //pointer to start of mmap'ed file data, or 0 if the file is not mmap'ed
    uchar *_fileMap;

    //incremented whenever 'add' is called and decremented whenever
    //'get' is called.
    //this is used to detect when a large number of lines are being read and processed from the history
    //and automatically mmap the file for better performance (saves the overhead of many lseek-read calls).
    int _readWriteBalance;

    //when _readWriteBalance goes below this threshold, the file will be mmap'ed automatically
    static const int MAP_THRESHOLD = -1000;
};

}

#endif
