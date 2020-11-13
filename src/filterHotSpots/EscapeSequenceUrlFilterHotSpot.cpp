/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
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
