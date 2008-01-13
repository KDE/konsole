/*
    Copyright 2007 by Robert Knight <robertknight@gmail.com>

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
#include "HistorySizeDialog.h"

// Qt
#include <QtGui/QButtonGroup>
#include <QtGui/QBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QRadioButton>
#include <QtGui/QSpinBox>
#include <QtGui/QWidget>

// KDE
#include <KLocalizedString>

// Konsole
#include "SessionManager.h"

using namespace Konsole;

HistorySizeDialog::HistorySizeDialog( QWidget* parent )
    :  KDialog(parent)
    ,  _noHistoryButton(0)
    ,  _fixedHistoryButton(0)
    ,  _unlimitedHistoryButton(0)
    ,  _lineCountBox(0)
    ,  _defaultMode(FixedSizeHistory)
    ,  _defaultLineCount(1000)
{
    // basic dialog properties
    setPlainCaption( i18n("Scrollback Options") );
    setButtons(  Default | Ok | Cancel );
    setDefaultButton(Ok);
    setModal( false );

    // dialog widgets
    QWidget* dialogWidget = new QWidget(this);
    setMainWidget(dialogWidget);

    QVBoxLayout* dialogLayout = new QVBoxLayout(dialogWidget);

    QButtonGroup* modeGroup = new QButtonGroup(this);

    _noHistoryButton = new QRadioButton( i18n("No scrollback") );
    _fixedHistoryButton = new QRadioButton( i18n("Fixed size scrollback: ") );
    _unlimitedHistoryButton = new QRadioButton( i18n("Unlimited scrollback") );

    modeGroup->addButton(_noHistoryButton);
    modeGroup->addButton(_fixedHistoryButton);
    modeGroup->addButton(_unlimitedHistoryButton);

    _lineCountBox = new QSpinBox(this);

    // minimum lines = 1 ( for 0 lines , "No History" mode should be used instead )
    // maximum lines is abritrarily chosen, I do not think it is sensible to allow this
    // to be set to a very large figure because that will use large amounts of memory,
    // if a very large log is required, "Unlimited History" mode should be used
    _lineCountBox->setRange( 1 , 100000 );

    // 1000 lines was the default in the KDE 3 series
    // using that for now
    _lineCountBox->setValue( _defaultLineCount );

    _lineCountBox->setSingleStep( _defaultLineCount / 10 );

    QLabel* lineCountLabel = new QLabel(i18n("lines"),this);
    QHBoxLayout* lineCountLayout = new QHBoxLayout();

    _fixedHistoryButton->setFocusProxy(_lineCountBox);

    connect( _fixedHistoryButton , SIGNAL(clicked()) , _lineCountBox , SLOT(selectAll()) );
    lineCountLayout->addWidget(_fixedHistoryButton);
    lineCountLayout->addWidget(_lineCountBox);
    lineCountLayout->addWidget(lineCountLabel);

    dialogLayout->addWidget(_noHistoryButton);
    dialogLayout->addLayout(lineCountLayout);
    dialogLayout->addWidget(_unlimitedHistoryButton);

    // select the fixed size mode by default
    _fixedHistoryButton->click();
    _fixedHistoryButton->setFocus( Qt::OtherFocusReason );

    connect(this,SIGNAL(defaultClicked()),this,SLOT(useDefaults()));

    connect(this,SIGNAL(accepted()),this,SLOT(emitOptionsChanged()));
}

void HistorySizeDialog::emitOptionsChanged()
{
    emit optionsChanged( mode() , lineCount() );
}

void HistorySizeDialog::setDefaultMode( HistoryMode mode ) { _defaultMode = mode; }
HistorySizeDialog::HistoryMode HistorySizeDialog::defaultMode() const { return _defaultMode; }
void HistorySizeDialog::setDefaultLineCount( int count ) { _defaultLineCount = count; }
int HistorySizeDialog::defaultLineCount() const { return _defaultLineCount; }

void HistorySizeDialog::useDefaults()
{
    setMode( _defaultMode );
    setLineCount( _defaultLineCount );
}

void HistorySizeDialog::setMode( HistoryMode mode )
{
    if ( mode == NoHistory )
    {
        _noHistoryButton->setChecked(true);
    } else if ( mode == FixedSizeHistory )
    {
        _fixedHistoryButton->setChecked(true);
    } else if ( mode == UnlimitedHistory )
    {
        _unlimitedHistoryButton->setChecked(true);
    }
}

HistorySizeDialog::HistoryMode HistorySizeDialog::mode() const
{
    if ( _noHistoryButton->isChecked() )
        return NoHistory;
    else if ( _fixedHistoryButton->isChecked() )
        return FixedSizeHistory;
    else if ( _unlimitedHistoryButton->isChecked() )
        return UnlimitedHistory;

    Q_ASSERT(false);
    return NoHistory;
}

int HistorySizeDialog::lineCount() const
{
    return _lineCountBox->value();
}

void HistorySizeDialog::setLineCount(int lines)
{
    _lineCountBox->setValue(lines);
}


#include "HistorySizeDialog.moc"
