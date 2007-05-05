/*
    This file is part of Konsole, an X terminal.
    Copyright (C) 2001 by Stephan Binner <binner@kde.org>

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

#ifndef KONSOLEIFACE_H
#define KONSOLEIFACE_H

#include <dcopobject.h>

class KonsoleIface : virtual public DCOPObject
{
    K_DCOP
    k_dcop:

    virtual int sessionCount() = 0;

    virtual QString currentSession() = 0;
    virtual QString newSession() = 0;
    virtual QString newSession(const QString &type) = 0;
    virtual QString sessionId(const int position) = 0;

    virtual void activateSession(const QString &sessionId) = 0;

    virtual void nextSession() = 0;
    virtual void prevSession() = 0;
    virtual void moveSessionLeft() = 0;
    virtual void moveSessionRight() = 0;
    virtual bool fullScreen() = 0;
    virtual void setFullScreen(bool on) = 0;
    virtual ASYNC reparseConfiguration() = 0;
};

#endif // KONSOLEIFACE_H
