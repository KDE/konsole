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

// Qt
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QWidget>

// KDE
#include <KLocalizedString>

// Konsole
#include "HistorySizeDialog.h"

using namespace Konsole;

HistorySizeDialog::HistorySizeDialog( QWidget* parent )
    :  KDialog(parent)
    ,  _mode( FixedSizeHistory )
    ,  _lineCount( 1000 )
{
    // basic dialog properties
    setPlainCaption( i18n("History Options") );
    setButtons(  Default | Ok | Cancel );
    setDefaultButton(Ok);
    setModal( true );

    // dialog widgets
    QWidget* dialogWidget = new QWidget(this);
    setMainWidget(dialogWidget);

    QVBoxLayout* dialogLayout = new QVBoxLayout(dialogWidget);

    QButtonGroup* modeGroup = new QButtonGroup(this);

    QRadioButton* noHistoryButton = new QRadioButton( i18n("No History") );
    QRadioButton* fixedHistoryButton = new QRadioButton( i18n("Fixed Size History") );
    QRadioButton* unlimitedHistoryButton = new QRadioButton( i18n("Unlimited History") );

    modeGroup->addButton(noHistoryButton);
    modeGroup->addButton(fixedHistoryButton);
    modeGroup->addButton(unlimitedHistoryButton);

    QSpinBox* lineCountBox = new QSpinBox(this);

    // minimum lines = 1 ( for 0 lines , "No History" mode should be used instead )
    // maximum lines is abritrarily chosen, I do not think it is sensible to allow this
    // to be set to a very large figure because that will use large amounts of memory,
    // if a very large log is required, "Unlimited History" mode should be used
    lineCountBox->setRange( 1 , 100000 );

    // 1000 lines was the default in the KDE 3 series
    // using that for now
    lineCountBox->setValue( 1000 );

    lineCountBox->setSingleStep( 100 );

    QLabel* lineCountLabel = new QLabel(i18n("lines"),this);
    QHBoxLayout* lineCountLayout = new QHBoxLayout();

    fixedHistoryButton->setFocusProxy(lineCountBox);

    connect( fixedHistoryButton , SIGNAL(clicked()) , lineCountBox , SLOT(selectAll()) );

    lineCountLayout->addWidget(fixedHistoryButton);
    lineCountLayout->addWidget(lineCountBox);
    lineCountLayout->addWidget(lineCountLabel);

    dialogLayout->addWidget(noHistoryButton);
    dialogLayout->addLayout(lineCountLayout);
    dialogLayout->addWidget(unlimitedHistoryButton);

    // select the fixed size mode by default
    fixedHistoryButton->click();
    fixedHistoryButton->setFocus( Qt::OtherFocusReason );
}

void HistorySizeDialog::setMode( HistoryMode mode )
{
    _mode = mode;
}

HistorySizeDialog::HistoryMode HistorySizeDialog::mode() const
{
    return _mode;
}

int HistorySizeDialog::lineCount() const
{
    return _lineCount;
}

void HistorySizeDialog::setLineCount(int lines)
{
    _lineCount = lines;
}


#include "HistorySizeDialog.moc"
