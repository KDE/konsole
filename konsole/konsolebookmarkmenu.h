/* This file was part of the KDE libraries
    Copyright (C) 2002 Carsten Pfeiffer <pfeiffer@kde.org>

    library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation, version 2.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef KONSOLEBOOKMARKMENU_H
#define KONSOLEBOOKMARKMENU_H

#include <qptrlist.h>
#include <kbookmark.h>
#include <kbookmarkmenu.h>

#include "konsolebookmarkhandler.h"


class QString;
class KBookmark;
class KAction;
class KActionMenu;
class KActionCollection;
class KBookmarkOwner;
class KBookmarkMenu;
class KPopupMenu;
class KonsoleBookmarkMenu;

class KonsoleBookmarkMenu : public KBookmarkMenu
{
    Q_OBJECT

public:
    KonsoleBookmarkMenu( KBookmarkManager* mgr,
                         KonsoleBookmarkHandler * _owner, KPopupMenu * _parentMenu,
                         KActionCollection *collec, bool _isRoot,
                         bool _add = true, const QString & parentAddress = "");

    void fillBookmarkMenu();

public slots:

signals:

private slots:

private:
    KonsoleBookmarkHandler * m_kOwner;

protected slots:
    void slotAboutToShow2();
    void slotBookmarkSelected();

protected:
    void refill();

private:
    class KonsoleBookmarkMenuPrivate;
    KonsoleBookmarkMenuPrivate *d;
};

#endif // KONSOLEBOOKMARKMENU_H
