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

// KDE
#include <KLocale>

// Konsole
#include "EditTabTitleFormatDialog.h"
#include "ui_EditTabTitleFormatDialog.h"

using namespace Konsole;

const EditTabTitleFormatDialog::Element EditTabTitleFormatDialog::_localElements[] = 
{
    { "%n" , i18n("Program Name") },
    { "%p" , i18n("Profile Name") },
    { "%d" , i18n("Current Directory (Short)") },
    { "%D" , i18n("Current Directory (Long)") }
};
const int EditTabTitleFormatDialog::_localElementCount = 4;
const EditTabTitleFormatDialog::Element EditTabTitleFormatDialog::_remoteElements[] =
{
    { "%p" , i18n("Profile Name") },
    { "%u" , i18n("User Name") },
    { "%h" , i18n("Remote Host (Short)") },
    { "%H" , i18n("Remote Host (Long)") }
};
const int EditTabTitleFormatDialog::_remoteElementCount = 4;

EditTabTitleFormatDialog::EditTabTitleFormatDialog(QWidget* parent)
    : KDialog(parent)
    , _context(Session::LocalTabTitle)
{
    setCaption(i18n("Edit Tab Title Format"));

    _ui = new Ui::EditTabTitleFormatDialog();
    _ui->setupUi(mainWidget());

    _ui->tabTitleFormatEdit->setClearButtonShown(true);

    connect( _ui->elementComboBox , SIGNAL(activated(int)) , this , SLOT(insertElement(int)) );
}
EditTabTitleFormatDialog::~EditTabTitleFormatDialog()
{
    delete _ui;
}
void EditTabTitleFormatDialog::insertElement(int index)
{
    if ( _context == Session::LocalTabTitle )
        _ui->tabTitleFormatEdit->insert( _localElements[index].element );
    else if ( _context == Session::RemoteTabTitle )
        _ui->tabTitleFormatEdit->insert( _remoteElements[index].element );
}
void EditTabTitleFormatDialog::setTabTitleFormat(const QString& format)
{
    _ui->tabTitleFormatEdit->setText(format);
    _ui->tabTitleFormatEdit->selectAll();
}
QString EditTabTitleFormatDialog::tabTitleFormat() const
{
    return _ui->tabTitleFormatEdit->text();
}
void EditTabTitleFormatDialog::setContext(Session::TabTitleContext context) 
{
    _context = context;

    _ui->elementComboBox->clear();

    QStringList list;
     
    if ( context == Session::LocalTabTitle )
    {
        for ( int i = 0 ; i < _localElementCount ; i++ )
            list << _localElements[i].description;
    }
    else if ( context == Session::RemoteTabTitle )
    {
        for ( int i = 0 ; i < _remoteElementCount ; i++ )
            list << _remoteElements[i].description;
    }
        
    _ui->elementComboBox->addItems( list );
}

#include "EditTabTitleFormatDialog.moc"
