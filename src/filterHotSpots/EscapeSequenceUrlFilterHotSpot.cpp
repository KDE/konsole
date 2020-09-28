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

#include "EscapeSequenceUrlFilterHotSpot.h"

#include <kio_version.h>
#if KIO_VERSION < QT_VERSION_CHECK(5, 71, 0)
#include <KRun>
#else
#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>
#endif

#include <QApplication>
#include <QMouseEvent>
#include <QDebug>

#include "terminalDisplay/TerminalDisplay.h"

using namespace Konsole;

EscapeSequenceUrlHotSpot::EscapeSequenceUrlHotSpot(
    int startLine,
    int startColumn,
    int endLine,
    int endColumn,
    const QString &text,
    const QString &url) :
    HotSpot(startLine, startColumn, endLine, endColumn),
    _text(text),
    _url(url)
{
    setType(EscapedUrl);
}

void EscapeSequenceUrlHotSpot::activate(QObject *obj)
{
    Q_UNUSED(obj)

#if KIO_VERSION < QT_VERSION_CHECK(5, 71, 0)
    new KRun(QUrl(_url), QApplication::activeWindow());
#else
    auto *job = new KIO::OpenUrlJob(QUrl(_url));
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, QApplication::activeWindow()));
    job->start();
#endif
}
