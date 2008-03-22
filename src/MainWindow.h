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
#include <KUrl>

// Local
#include "Profile.h"

class KToggleAction;

namespace Konsole
{

class IncrementalSearchBar;
class ViewManager;
class ViewProperties;
class SessionController;
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

        /**
         * Sets the default profile for this window.
         * This is the default value for the profile argument
         * when the newSessionRequest() and newWindow() signals 
         * are emitted.
         */
        void setDefaultProfile(Profile::Ptr profile);

        /**
         * Returns the default profile for this window.
         * See setDefaultProfile()
         */
        Profile::Ptr defaultProfile() const;

		
    signals:
        /** 
         * Emitted by the main window to request the creation of a new session.
         *
         * @param profile The profile to use to create the new session.
         * @param directory Initial working directory for the new session or empty 
         * if the default working directory associated with the profile should be used.
         * @param view The view manager owned by this main window 
         */
        void newSessionRequest(Profile::Ptr profile,
                               const QString& directory,
                               ViewManager* view);

        /**
         * Emitted by the main window to request the creation of a 
         * new session in a new window.
         *
         * @param profile The profile to use to create the 
         * first session in the new window.
         * @param directory Initial working directory for the new window or empty
         * if the default working directory associated with the profile should
         * be used.
         */
        void newWindowRequest(Profile::Ptr profile,
                              const QString& directory);

        /**
         * Emitted by the main window to request the current session to close.
         */
        void closeActiveSessionRequest();

    protected:
        // reimplemented from KMainWindow
        virtual bool queryClose();

    private slots:
        void newTab();
        void newWindow();
        void showManageProfilesDialog();
        void showRemoteConnectionDialog();
        void showShortcutsDialog();
        void newFromProfile(Profile::Ptr profile);
        void activeViewChanged(SessionController* controller);
        void activeViewTitleChanged(ViewProperties*);

        void sessionListChanged(const QList<QAction*>& actions);
        void viewFullScreen(bool fullScreen);
        void configureNotifications();

		// single shot call to set the visibility of the menu bar.  Has no 
		// effect if the menu bar is a MacOS-style top-level menu
		void setMenuBarVisibleOnce(bool visible);

		void openUrls(const QList<KUrl>& urls);

    private:
        void correctShortcuts();
        void setupActions();
        void setupWidgets();
        QString activeSessionDir() const;
		void disconnectController(SessionController* controller);

    private:
        ViewManager*  _viewManager;
        BookmarkHandler* _bookmarkHandler;
        IncrementalSearchBar* _searchBar;
        KToggleAction* _toggleMenuBarAction;

        QPointer<SessionController> _pluggedController;

        Profile::Ptr _defaultProfile;
		bool _menuBarVisibilitySet;
};

}

#endif // KONSOLEMAINWINDOW_H
