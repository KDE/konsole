/* This file wass part of the KDE libraries
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

// Born as kdelibs/kio/kfile/kfilebookmarkhandler.h

#ifndef KONSOLEBOOKMARKHANDLER_H
#define KONSOLEBOOKMARKHANDLER_H

#include <kbookmarkmanager.h>
#include "konsolebookmarkmenu.h"

class KPopupMenu;
class KonsoleBookmarkMenu;
class KBookmarkManager;

class KonsoleBookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    KonsoleBookmarkHandler( Konsole *konsole, bool toplevel );
    ~KonsoleBookmarkHandler();

    QPopupMenu * popupMenu();

    // KBookmarkOwner interface:
    virtual void openBookmarkURL( const QString& url, const QString& title )
                                { emit openURL( url, title ); }
    virtual QString currentURL() const;
    virtual QString currentTitle() const;

    KPopupMenu *menu() const { return m_menu; }

private slots:
    void slotBookmarksChanged( const QString &, const QString & caller );

signals:
    void openURL( const QString& url, const QString& title );

private:
    Konsole *m_konsole;
    KPopupMenu *m_menu;
    KonsoleBookmarkMenu *m_bookmarkMenu;
    QString m_file;
};

#endif // KONSOLEBOOKMARKHANDLER_H
