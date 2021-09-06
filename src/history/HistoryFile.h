/*
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
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
    virtual void removeLast(qint64 loc);
    virtual qint64 len() const;

    // mmaps the file in read-only mode
    void map();
    // un-mmaps the file
    void unmap();

private:
    qint64 _length;
    QTemporaryFile _tmpFile;

    // pointer to start of mmap'ed file data, or 0 if the file is not mmap'ed
    uchar *_fileMap;

    // incremented whenever 'add' is called and decremented whenever
    //'get' is called.
    // this is used to detect when a large number of lines are being read and processed from the history
    // and automatically mmap the file for better performance (saves the overhead of many lseek-read calls).
    int _readWriteBalance;

    // when _readWriteBalance goes below this threshold, the file will be mmap'ed automatically
    static const int MAP_THRESHOLD = -1000;
};

}

#endif
