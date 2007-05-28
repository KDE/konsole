/*
    Copyright (C) 2006-2007 by Robert Knight <robertknight@gmail.com>

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

#ifndef KONSOLEMAINWINDOW_H
#define KONSOLEMAINWINDOW_H

// Qt
#include <QtCore/QPointer>

// KDE
#include <KXmlGuiWindow>

class KToggleAction;

namespace Konsole
{

class IncrementalSearchBar;
class ViewSplitter;
class ViewManager;
class ViewProperties;
class SessionController;
class Profile;
class ProfileList;
class BookmarkHandler;

/**
 * The main window.  This contains the menus and an area which contains the terminal displays.
 *
 * The main window does not create the views or the container widgets which hold the views.
 * This is done by the ViewManager class.  When a MainWindow is instantiated, it creates
 * a new ViewManager.  The ViewManager can then be used to create new terminal displays
 * inside the window.
 *
 * Do not construct new main windows directly, use Application's newMainWindow() method.
 */
class MainWindow : public KXmlGuiWindow
{
    Q_OBJECT

    public:
        /** 
         * Constructs a new main window.  Do not create new main windows directly, use Application's
         * newMainWindow() method instead.
         */
        MainWindow();

        /**
         * Returns the view manager associated with this window.  The view manager can be used to 
         * create new views on particular session objects inside this window.
         */
        ViewManager* viewManager() const;

        /**
         * Returns the search bar.
         * TODO - More documentation
         */
        IncrementalSearchBar* searchBar() const;

        /** Sets the list of sessions to be displayed in the File menu */
        void setSessionList(ProfileList* list);

        /**
         * Returns the bookmark handler associated with this window.
         */
        BookmarkHandler* bookmarkHandler() const;

    signals:
        /** 
         * Emitted by the main window to request the creation of a new session.
         *
         * @param key The key for the profile to use to create the new session.
         * @param view The view manager owned by this main window 
         */
        void newSessionRequest(const QString& key , ViewManager* view);

        /**
         * Emitted by the main window to request the creation of a 
         * new session in a new window.
         *
         * @param key The key for the profile to use to create the 
         * first session in the new window.
         */
        void newWindowRequest(const QString& key);

    private slots:
        void newTab();
        void newWindow();
        void showManageProfilesDialog();
        void showRemoteConnectionDialog();
        void showShortcutsDialog();
        void newFromProfile(const QString&);
        void activeViewChanged(SessionController* controller);
        void activeViewTitleChanged(ViewProperties*);

        void sessionListChanged(const QList<QAction*>& actions);
        void viewFullScreen(bool fullScreen);

    private:
        void setupActions();
        void setupWidgets();

    private:
        ViewManager*  _viewManager;
        BookmarkHandler* _bookmarkHandler;
        IncrementalSearchBar* _searchBar;
        KToggleAction* _toggleMenuBarAction;

        QPointer<SessionController> _pluggedController;
};

}

#endif // KONSOLEMAINWINDOW_H
