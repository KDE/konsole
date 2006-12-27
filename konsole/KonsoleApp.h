/*
    Copyright (C) 2006 by Robert Knight <robertknight@gmail.com>

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

// KDE 
#include <KUniqueApplication>

class KCmdLineArgs;
class SessionManager;

/**
 * The Konsole Application
 */
class KonsoleApp : public KUniqueApplication
{
public:
    /** Constructs a new Konsole application. */
    KonsoleApp();

    /** Creates a new main window and opens a default terminal session */
    virtual int newInstance();

    /** Returns the KonsoleApp instance */
    static KonsoleApp* self();

    /** Returns the session manager */
    SessionManager* sessionManager();
        
private:
    KCmdLineArgs*   _arguments;
    SessionManager* _sessionManager;
      
};
