/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "EscapeSequenceUrlFilterHotSpot.h"

#include <kio_version.h>
#if KIO_VERSION >= QT_VERSION_CHECK(5, 98, 0)
#include <KIO/JobUiDelegateFactory>
#else
#include <KIO/JobUiDelegate>
#endif
#include <KIO/OpenUrlJob>

#include <QApplication>
#include <QDebug>
#include <QMouseEvent>

#include "terminalDisplay/TerminalDisplay.h"

using namespace Konsole;

EscapeSequenceUrlHotSpot::EscapeSequenceUrlHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QString &text, const QString &url)
    : HotSpot(startLine, startColumn, endLine, endColumn)
    , _text(text)
    , _url(url)
{
    setType(EscapedUrl);
}

void EscapeSequenceUrlHotSpot::activate(QObject *obj)
{
    Q_UNUSED(obj)

    auto *job = new KIO::OpenUrlJob(QUrl(_url));
#if KIO_VERSION >= QT_VERSION_CHECK(5, 98, 0)
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, QApplication::activeWindow()));
#else
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, QApplication::activeWindow()));
#endif
    job->start();
}
