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
//Added by qt3to4:
#include <QTextStream>
#include <QMenu>

class KMenu;
class KBookmarkMenu;
class KBookmarkManager;
class KMainWindow;

class KonsoleBookmarkHandler : public QObject, public KBookmarkOwner
{
    Q_OBJECT

public:
    KonsoleBookmarkHandler( KMainWindow* konsole, KMenu* menu, bool toplevel );
    ~KonsoleBookmarkHandler();

    QMenu * popupMenu();

    virtual QString currentUrl() const;
    virtual QString currentTitle() const;
    virtual bool addBookmarkEntry() const;
    virtual bool editBookmarkEntry() const;

    KMenu *menu() const { return m_menu; }

Q_SIGNALS:
    void openUrl( const QString&, const QString& );

private Q_SLOTS:
    void openBookmark( const KBookmark & bm, Qt::MouseButtons, Qt::KeyboardModifiers );

private:
    KMainWindow *m_konsole;
    KMenu *m_menu;
    KBookmarkMenu *m_bookmarkMenu;
    QString m_file;
    bool m_toplevel;
};

#endif // KONSOLEBOOKMARKHANDLER_H
