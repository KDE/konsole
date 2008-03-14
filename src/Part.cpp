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
#include <KAction>
#include <KActionCollection>
#include <KDebug>
#include <KLocale>
#include <KWindowSystem>
#include <kdeversion.h>

// Konsole
#include "ColorScheme.h"
#include "EditProfileDialog.h"
#include "Emulation.h"
#include "KeyboardTranslator.h"
#include "ManageProfilesDialog.h"
#include "Session.h"
#include "SessionController.h"
#include "SessionManager.h"
#include "TerminalDisplay.h"
#include "ViewManager.h"
#include "MainWindow.h"

// X
#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/extensions/Xrender.h>
#endif

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
  ,_manageProfilesAction(0)
{
    TerminalDisplay::HAVE_TRANSPARENCY = transparencyAvailable();

	// setup global actions
	createGlobalActions();

    // create view widget
    _viewManager = new ViewManager(this,actionCollection());
    _viewManager->setNavigationMethod( ViewManager::NoNavigation );

    connect( _viewManager , SIGNAL(activeViewChanged(SessionController*)) , this ,
           SLOT(activeViewChanged(SessionController*)) );
    connect( _viewManager , SIGNAL(empty()) , this , SLOT(terminalExited()) );
    connect( _viewManager , SIGNAL(newViewRequest()) , this , SLOT(newTab()) );

    _viewManager->widget()->setParent(parentWidget);

    setWidget(_viewManager->widget());
    actionCollection()->addAssociatedWidget(_viewManager->widget());
    foreach (QAction* action, actionCollection()->actions())
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);

    // create basic session
    createSession(QString());
}
Part::~Part()
{
}
void Part::createGlobalActions()
{
	_manageProfilesAction = new QAction(i18n("Manage Profiles..."),this);
	connect(_manageProfilesAction,SIGNAL(triggered()),this,SLOT(showManageProfilesDialog()));
}
void Part::setupActionsForSession(SessionController* session)
{
	KActionCollection* collection = session->actionCollection();
	collection->addAction("manage-profiles",_manageProfilesAction);
}
bool Part::transparencyAvailable()
{
#ifdef Q_WS_X11
    bool ARGB = false;

    int screen = QX11Info::appScreen();
    bool depth = (QX11Info::appDepth() == 32);

    Display* display = QX11Info::display();
    Visual* visual = static_cast<Visual*>(QX11Info::appVisual(screen));

    XRenderPictFormat* format = XRenderFindVisualFormat(display, visual);

    if (depth && format->type == PictTypeDirect && format->direct.alphaMask)
    {
        ARGB = true;
    }

    if (ARGB)
    {
        return KWindowSystem::compositingActive();
    }
    else
#endif
    {
        return false;
    }
}

bool Part::openFile()
{
    return false;
}
void Part::terminalExited()
{
    deleteLater();
}
void Part::newTab()
{
    createSession( QString() );
    showShellInDir( QString() );
}
Session* Part::activeSession() const
{
	if ( _viewManager->activeViewController() )
	{
		Q_ASSERT( _viewManager->activeViewController()->session());

		return _viewManager->activeViewController()->session();	
	}
	else
	{
		return 0;
	}
}
void Part::startProgram( const QString& program,
                           const QStringList& arguments )
{
	Q_ASSERT( activeSession() );

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
	Q_ASSERT( activeSession() );

    if ( !activeSession()->isRunning() )
    {
        if ( !dir.isEmpty() )
            activeSession()->setInitialWorkingDirectory(dir);
        activeSession()->run();
    }
}
void Part::sendInput( const QString& text )
{
	Q_ASSERT( activeSession() );
	activeSession()->emulation()->sendText(text);
}

Session* Part::createSession(const QString& key)
{
    Session* session = SessionManager::instance()->createSession(key);
    _viewManager->createView(session);

    return session;
}
void Part::activeViewChanged(SessionController* controller)
{
	Q_ASSERT( controller );
	Q_ASSERT( controller->view() );

    widget()->setFocusProxy( controller->view() );

	// remove existing controller
    if (_pluggedController) 
	{
		removeChildClient (_pluggedController);
		disconnect(_pluggedController,SIGNAL(titleChanged(ViewProperties*)),this,
					SLOT(activeViewTitleChanged(ViewProperties*)));
	}

	// insert new controller
	setupActionsForSession(controller);
    insertChildClient(controller);
	connect(controller,SIGNAL(titleChanged(ViewProperties*)),this,
			SLOT(activeViewTitleChanged(ViewProperties*)));
	activeViewTitleChanged(controller);

    _pluggedController = controller;
}
void Part::activeViewTitleChanged(ViewProperties* properties)
{
	emit setWindowCaption(properties->title());
}
void Part::showManageProfilesDialog()
{
	showManageProfilesDialog(_viewManager->widget());
}
void Part::showManageProfilesDialog(QWidget* parent)
{
	ManageProfilesDialog* dialog = new ManageProfilesDialog(parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setShortcutEditorVisible(false);
	dialog->show();
}
void Part::showEditCurrentProfileDialog(QWidget* parent)
{
	Q_ASSERT( activeSession() );

	EditProfileDialog* dialog = new EditProfileDialog(parent);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->setProfile( activeSession()->profileKey() );
	dialog->show();	
}
void Part::changeSessionSettings(const QString& text)
{
	// send a profile change command, the escape code format 
	// is the same as the normal X-Term commands used to change the window title or icon,
	// but with a magic value of '50' for the parameter which specifies what to change
	Q_ASSERT( activeSession() );
	QByteArray buffer;
	buffer.append("\033]50;").append(text.toUtf8()).append('\a');
	
	activeSession()->emulation()->receiveData(buffer.constData(),buffer.length());  
}

#include "Part.moc"
