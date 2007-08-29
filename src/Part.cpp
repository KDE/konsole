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

// Own
#include "Part.h"

// Qt
#include <QtCore/QStringList>

// KDE
#include <KMenuBar>
#include <KXMLGUIFactory>

// Konsole
#include "ColorScheme.h"
#include "Emulation.h"
#include "KeyboardTranslator.h"
#include "Session.h"
#include "SessionController.h"
#include "SessionManager.h"
#include "ViewManager.h"
#include "MainWindow.h"

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

KParts::Part* PartFactory::createPartObject( QWidget* parentWidget,
                                             QObject* parent,
                                             const char* /*classname*/,
                                             const QStringList& /*args*/)
{
    return new Part(parentWidget,parent);
}

K_EXPORT_PLUGIN(Konsole::PartFactory())

Part::Part(QWidget* parentWidget , QObject* parent)
 : KParts::ReadOnlyPart(parent)
  ,_viewManager(0)
  ,_pluggedController(0)
{
    // setup global managers
    if ( SessionManager::instance() == 0 )
        SessionManager::setInstance( new SessionManager() );
    if ( ColorSchemeManager::instance() == 0 )
        ColorSchemeManager::setInstance( new ColorSchemeManager() );
    if ( KeyboardTranslatorManager::instance() == 0 )
        KeyboardTranslatorManager::setInstance( new KeyboardTranslatorManager() );

    // create view widget
    _viewManager = new ViewManager(this,actionCollection());

    connect( _viewManager , SIGNAL(activeViewChanged(SessionController*)) , this ,
           SLOT(activeViewChanged(SessionController*)) ); 

    connect( _viewManager , SIGNAL(empty()) , this , SLOT(restart()) );

    _viewManager->widget()->setParent(parentWidget);

    setWidget(_viewManager->widget());
    
    // create basic session
    createSession(QString());
}
Part::~Part()
{
    // disable creation of new sessions when the last one is closed
    disconnect( _viewManager , SIGNAL(empty()) , this , SLOT(restart()) );
}
bool Part::openFile()
{
    return false;
}
void Part::restart()
{
    createSession( QString() );
    showShellInDir( QString() );
}
Session* Part::activeSession() const
{
    if ( _pluggedController )
    {
        qDebug() << __FUNCTION__ << " - have plugged controller";

        return _pluggedController->session();
    }
    else
    {
        // for now, just return the first available session
        QList<Session*> list = SessionManager::instance()->sessions();

        qDebug() << __FUNCTION__ << " - no plugged controller, selectin first from" << list.count() << "sessions";
        
        Q_ASSERT( !list.isEmpty() );

        return list.first();
    }
}
void Part::startProgram( const QString& program,
                           const QStringList& arguments )
{
    if ( !activeSession()->isRunning() )
    {
        if ( !program.isEmpty() && !arguments.isEmpty() )
        {
            activeSession()->setProgram(program);
            activeSession()->setArguments(arguments);
        }

        activeSession()->run();
    }
}
void Part::showShellInDir( const QString& dir )
{
    if ( !activeSession()->isRunning() )
    {
        if ( !dir.isEmpty() )
            activeSession()->setInitialWorkingDirectory(dir);
        activeSession()->run();
    }
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
