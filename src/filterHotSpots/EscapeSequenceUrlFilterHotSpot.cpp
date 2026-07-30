/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "EscapeSequenceUrlFilterHotSpot.h"

#include <KIO/JobUiDelegateFactory>
#include <KIO/OpenUrlJob>
#include <KLocalizedString>

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDrag>
#include <QMimeData>
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

QMimeData *EscapeSequenceUrlHotSpot::createMimeData() const
{
    auto *mimeData = new QMimeData();
    mimeData->setText(_url);
    mimeData->setUrls({QUrl(_url)});
    return mimeData;
}

void EscapeSequenceUrlHotSpot::activate(QObject *object)
{
    const QString actionName = object != nullptr ? object->objectName() : QString();

    if (actionName == QLatin1String("copy-action")) {
        auto *mimeData = createMimeData();
        QApplication::clipboard()->setMimeData(mimeData);
        return;
    }

    if ((object == nullptr) || actionName == QLatin1String("open-action")) {
        auto *job = new KIO::OpenUrlJob(QUrl(_url));
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, QApplication::activeWindow()));
        job->start();
    }
}

QList<QAction *> EscapeSequenceUrlHotSpot::actions()
{
    auto openAction = new QAction(this);
    auto copyAction = new QAction(this);

    openAction->setText(i18n("Open Link"));
    openAction->setIcon(QIcon::fromTheme(QStringLiteral("internet-services")));
    copyAction->setText(i18n("Copy Link Address"));
    copyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-url")));

    // object names are set here so that the hotspot performs the
    // correct action when activated() is called with the triggered
    // action passed as a parameter.
    openAction->setObjectName(QStringLiteral("open-action"));
    copyAction->setObjectName(QStringLiteral("copy-action"));

    QObject::connect(openAction, &QAction::triggered, this, [this, openAction] {
        activate(openAction);
    });
    QObject::connect(copyAction, &QAction::triggered, this, [this, copyAction] {
        activate(copyAction);
    });

    return {openAction, copyAction};
}

bool EscapeSequenceUrlHotSpot::hasDragOperation() const
{
    return true;
}

void EscapeSequenceUrlHotSpot::startDrag()
{
    auto *drag = new QDrag(this);
    auto *mimeData = createMimeData();

    drag->setMimeData(mimeData);
    // TODO add drag pixmap containing the URL.
    drag->exec(Qt::CopyAction);
}

void EscapeSequenceUrlHotSpot::mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    HotSpot::mouseEnterEvent(td, ev);

    td->setHoverLinkIndicator(this->_url);
}

void EscapeSequenceUrlHotSpot::mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    HotSpot::mouseLeaveEvent(td, ev);

    td->setHoverLinkIndicator(QStringLiteral(""));
}
