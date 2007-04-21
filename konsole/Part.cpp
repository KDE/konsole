/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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

// Qt
#include <QStringList>

// KDE
#include <KXMLGUIFactory>

// Konsole
#include "ColorScheme.h"
#include "KeyTrans.h"
#include "Part.h"
#include "Session.h"
#include "SessionController.h"
#include "SessionManager.h"
#include "ViewManager.h"

extern "C"
{
    // entry point for Konsole part library,
    // returns a new factory which can be used to construct Konsole parts
    KDE_EXPORT void* init_libkonsolepart()
    {
        return new Konsole::PartFactory;
    }
}

using namespace Konsole;

KParts::Part* PartFactory::createPartObject( QWidget* /*parentWidget*/,
                                             QObject* parent,
                                             const char* /*classname*/,
                                             const QStringList& /*args*/)
{
    return new Part(parent);
}

Part::Part(QObject* parent)
 : KParts::ReadOnlyPart(parent)
  ,_viewManager(0)
  ,_pluggedController(0)
{
    // setup global managers
    if ( SessionManager::instance() == 0 )
        SessionManager::setInstance( new SessionManager() );
    if ( ColorSchemeManager::instance() == 0 )
        ColorSchemeManager::setInstance( new ColorSchemeManager() );
    KeyTrans::loadAll();

    // create window and session for part
    _viewManager = new ViewManager(this,actionCollection());
    connect( _viewManager , SIGNAL(activeViewChanged(SessionController*)) , this ,
           SLOT(activeViewChanged(SessionController*)) ); 
    createSession( QString() );

    setWidget(_viewManager->widget());
}
Part::~Part()
{
}
bool Part::openFile()
{
    return false;
}
void Part::startProgram( const QString& program,
                           const QStringList& arguments )
{
    Session* session = createSession(QString::null);

    session->setProgram(program);
    session->setArguments(arguments);
   
    session->run();
}
void Part::showShellInDir( const QString& dir )
{
    Session* session = createSession(QString::null);

    session->setInitialWorkingDirectory(dir);
    
    session->run();
}
void Part::sendInput( const QString& text )
{
    // currently sends input to every running session,
    // the alternative would be to send only to the active session
    QListIterator<Session*> iter( SessionManager::instance()->sessions() );
    while ( iter.hasNext() )
        iter.next()->emulation()->sendText(text);
}
Session* Part::createSession(const QString& key)
{
    Session* session = SessionManager::instance()->createSession(key);
    session->setListenToKeyPress(true);

    _viewManager->createView(session);

    return session;
}
void Part::activeViewChanged(SessionController* controller)
{
    if ( controller == _pluggedController )
        return;

    if ( factory() )
    {
        factory()->removeClient(_pluggedController);
        factory()->addClient(controller);
    }

    _pluggedController = controller;
}

#include "Part.moc"
