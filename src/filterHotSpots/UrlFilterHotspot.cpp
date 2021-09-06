/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "UrlFilterHotspot.h"

#include <KIO/JobUiDelegate>
#include <KIO/OpenUrlJob>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QRegularExpression>

#include <KLocalizedString>
#include <QIcon>
#include <QMouseEvent>

#include "UrlFilter.h"
#include "terminalDisplay/TerminalDisplay.h"
// regexp matches:
// full url:

using namespace Konsole;

UrlFilterHotSpot::~UrlFilterHotSpot() = default;

UrlFilterHotSpot::UrlFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts)
    : RegExpFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts)
{
    const UrlType kind = urlType();
    if (kind == Email) {
        setType(EMailAddress);
    } else {
        setType(Link);
    }
}

UrlFilterHotSpot::UrlType UrlFilterHotSpot::urlType() const
{
    const QString url = capturedTexts().at(0);

    // Don't use a ternary here, it gets completely unreadable
    if (UrlFilter::FullUrlRegExp.match(url).hasMatch()) {
        return StandardUrl;
    } else if (UrlFilter::EmailAddressRegExp.match(url).hasMatch()) {
        return Email;
    } else {
        return Unknown;
    }
}

void UrlFilterHotSpot::activate(QObject *object)
{
    QString url = capturedTexts().at(0);

    const UrlType kind = urlType();

    const QString &actionName = object != nullptr ? object->objectName() : QString();

    if (actionName == QLatin1String("copy-action")) {
        QApplication::clipboard()->setText(url);
        return;
    }

    if ((object == nullptr) || actionName == QLatin1String("open-action")) {
        if (kind == StandardUrl) {
            // if the URL path does not include the protocol ( eg. "www.kde.org" ) then
            // prepend https:// ( eg. "www.kde.org" --> "https://www.kde.org" )
            if (!url.contains(QLatin1String("://"))) {
                url.prepend(QLatin1String("https://"));
            }
        } else if (kind == Email) {
            url.prepend(QLatin1String("mailto:"));
        }

        auto *job = new KIO::OpenUrlJob(QUrl(url));
        job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, QApplication::activeWindow()));
        job->start();
    }
}

QList<QAction *> UrlFilterHotSpot::actions()
{
    auto openAction = new QAction(this);
    auto copyAction = new QAction(this);

    const UrlType kind = urlType();
    Q_ASSERT(kind == StandardUrl || kind == Email);

    if (kind == StandardUrl) {
        openAction->setText(i18n("Open Link"));
        openAction->setIcon(QIcon::fromTheme(QStringLiteral("internet-services")));
        copyAction->setText(i18n("Copy Link Address"));
        copyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-url")));
    } else if (kind == Email) {
        openAction->setText(i18n("Send Email To..."));
        openAction->setIcon(QIcon::fromTheme(QStringLiteral("mail-send")));
        copyAction->setText(i18n("Copy Email Address"));
        copyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-mail")));
    }

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
