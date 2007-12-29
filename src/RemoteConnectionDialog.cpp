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
#include "RemoteConnectionDialog.h"

// Qt
#include <KDebug>

// KDE
#include <KLocale>

// Konsole
#include "SessionManager.h"

#include "ui_RemoteConnectionDialog.h"

using namespace Konsole;

RemoteConnectionDialog::RemoteConnectionDialog(QWidget* parent)
    : KDialog(parent)
{
    setCaption(i18n("New Remote Connection"));
    setButtons( KDialog::Ok | KDialog::Cancel );
    setButtonText( KDialog::Ok , i18n("Connect") );

    _ui = new Ui::RemoteConnectionDialog();
    _ui->setupUi(mainWidget());

    // set initial UI state
    _ui->userEdit->setFocus(Qt::OtherFocusReason);
}
RemoteConnectionDialog::~RemoteConnectionDialog()
{
    delete _ui;
}
QString RemoteConnectionDialog::user() const
{
    return _ui->userEdit->text();
}
QString RemoteConnectionDialog::host() const
{
    return _ui->hostEdit->text();
}
QString RemoteConnectionDialog::service() const
{
    return "ssh";
}

QString RemoteConnectionDialog::sessionKey() const
{
 //   SessionManager* manager = SessionManager::instance();

    /*MutableSessionInfo* customSession = 
        new MutableSessionInfo(manager->defaultSessionType()->path());
    customSession->setCommand( service() );
    customSession->setName( i18n("%1 at %2",user(),host()) );
    customSession->setArguments( QStringList() << customSession->command(true,true) << 
            user() + '@' + host() );
*/
   //QString key = manager->addSessionType( customSession ); 

    //kDebug() << "session key = " << key;

    return QString();
    //return key;
}

