/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2002 Carsten Pfeiffer <pfeiffer@kde.org>
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Born as kdelibs/kio/kfile/kfilebookmarkhandler.cpp

// Own
#include "BookmarkHandler.h"

// Qt
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

// KDE
#include <KLocalizedString>
#include <KShell>

// Konsole
#include "BookmarkMenu.h"
#include "ViewProperties.h"

using namespace Konsole;

BookmarkHandler::BookmarkHandler(KActionCollection *collection, QMenu *menu, bool toplevel, QObject *parent)
    : QObject(parent)
    , KBookmarkOwner()
    , _menu(menu)
    , _file(QString())
    , _toplevel(toplevel)
    , _activeView(nullptr)
    , _views(QList<ViewProperties *>())
{
    setObjectName(QStringLiteral("BookmarkHandler"));

    _file = QStandardPaths::locate(QStandardPaths::GenericDataLocation, QStringLiteral("konsole/bookmarks.xml"));

    if (_file.isEmpty()) {
        _file = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/konsole");
        QDir().mkpath(_file);
        _file += QStringLiteral("/bookmarks.xml");
    }

    KBookmarkManager *manager = KBookmarkManager::managerForFile(_file, QStringLiteral("konsole"));
    manager->setUpdate(true);

    auto *bookmarkMenu = new BookmarkMenu(manager, this, _menu, toplevel ? collection : nullptr);
    bookmarkMenu->setParent(this);
}

BookmarkHandler::~BookmarkHandler() = default;

void BookmarkHandler::openBookmark(const KBookmark &bm, Qt::MouseButtons, Qt::KeyboardModifiers)
{
    Q_EMIT openUrl(bm.url());
}

void BookmarkHandler::openFolderinTabs(const KBookmarkGroup &group)
{
    Q_EMIT openUrls(group.groupUrlList());
}

bool BookmarkHandler::enableOption(BookmarkOption option) const
{
    if (option == ShowAddBookmark || option == ShowEditBookmark) {
        return _toplevel;
    }
    return KBookmarkOwner::enableOption(option);
}

QUrl BookmarkHandler::currentUrl() const
{
    return urlForView(_activeView);
}

QUrl BookmarkHandler::urlForView(ViewProperties *view) const
{
    if (view != nullptr) {
        return view->url();
    }
    return {};
}

QString BookmarkHandler::currentTitle() const
{
    return titleForView(_activeView);
}

QString BookmarkHandler::titleForView(ViewProperties *view) const
{
    const QUrl &url = view != nullptr ? view->url() : QUrl();
    if (url.isLocalFile()) {
        QString path = url.path();

        path = KShell::tildeExpand(path);
        path = QFileInfo(path).completeBaseName();

        return path;
    }
    if (!url.host().isEmpty()) {
        if (!url.userName().isEmpty()) {
            return i18nc("@item:inmenu The user's name and host they are connected to via ssh", "%1 on %2", url.userName(), url.host());
        }
        return i18nc("@item:inmenu The host the user is connected to via ssh", "%1", url.host());
    }

    return url.toDisplayString();
}

QString BookmarkHandler::currentIcon() const
{
    return iconForView(_activeView);
}

QString BookmarkHandler::iconForView(ViewProperties *view) const
{
    if (view != nullptr) {
        return view->icon().name();
    }
    return {};
}

QList<KBookmarkOwner::FutureBookmark> BookmarkHandler::currentBookmarkList() const
{
    QList<KBookmarkOwner::FutureBookmark> list;
    list.reserve(_views.size());

    const QList<ViewProperties *> views = _views;
    for (ViewProperties *view : views) {
        list << KBookmarkOwner::FutureBookmark(titleForView(view), urlForView(view), iconForView(view));
    }

    return list;
}

bool BookmarkHandler::supportsTabs() const
{
    return true;
}

void BookmarkHandler::setViews(const QList<ViewProperties *> &views)
{
    _views = views;
}

QList<ViewProperties *> BookmarkHandler::views() const
{
    return _views;
}

void BookmarkHandler::setActiveView(ViewProperties *view)
{
    _activeView = view;
}

ViewProperties *BookmarkHandler::activeView() const
{
    return _activeView;
}
