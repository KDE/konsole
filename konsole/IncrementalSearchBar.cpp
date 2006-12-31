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

// Qt
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QToolButton>

// KDE
#include <KLocale>
#include <KIcon>

// Konsole
#include "IncrementalSearchBar.h"

IncrementalSearchBar::IncrementalSearchBar(Features features , QWidget* parent)
    : QWidget(parent)
    , _foundMatch(false)
    , _matchCase(false)
    , _matchRegExp(false)
    , _highlightMatches(false)
    , _searchEdit(0)
    , _continueLabel(0)
{
    QHBoxLayout* layout = new QHBoxLayout(parent);
  
    QToolButton* close = new QToolButton(this);
    close->setAutoRaise(true);
    close->setIcon(KIcon("fileclose"));
    connect( close , SIGNAL(clicked()) , this , SIGNAL(closeClicked()) );

    QLabel* findLabel = new QLabel(i18n("Find"),this);
    _searchEdit = new QLineEdit(this);

    // text box may be a minimum of 3 characters wide and a maximum of 10 characters wide
    // (since the maxWidth metric is used here, more characters probably will fit in than 3
    //  and 10)
    QFontMetrics metrics(_searchEdit->font());
    int maxWidth = metrics.maxWidth();
    _searchEdit->setMinimumWidth(maxWidth*3);
    _searchEdit->setMaximumWidth(maxWidth*10);

    connect( _searchEdit , SIGNAL(textChanged(const QString&)) , this , SIGNAL(searchChanged(const QString&)));

    QToolButton* findNext = new QToolButton(this);
    findNext->setText(i18n("Next"));
    findNext->setAutoRaise(true);
    findNext->setIcon( KIcon("next") );
    findNext->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect( findNext , SIGNAL(clicked()) , this , SIGNAL(findNextClicked()) );

    QToolButton* findPrev = new QToolButton(this);
    findPrev->setText(i18n("Previous"));
    findPrev->setAutoRaise(true);
    findPrev->setIcon( KIcon("previous") );
    findPrev->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    connect( findPrev , SIGNAL(clicked()) , this , SIGNAL(findPreviousClicked()) );

    QCheckBox* highlightMatches = 0;
    if ( features & HighlightMatches )
    {
        highlightMatches = new QCheckBox( i18n("Highlight Matches") , this );
        connect( highlightMatches , SIGNAL(toggled(bool)) , this , 
                 SIGNAL(highlightMatchesToggled(bool)) );
    }

    QCheckBox* matchCase = 0;
    if ( features & MatchCase )
    {
        matchCase = new QCheckBox( i18n("Match Case") , this );
        connect( matchCase , SIGNAL(toggled(bool)) , this , SIGNAL(matchCaseToggled(bool)) );
    }

    QCheckBox* matchRegExp = 0;
    if ( features & RegExp )
    {
        matchRegExp = new QCheckBox( i18n("Match Regular Expression") , this );
        connect( matchRegExp , SIGNAL(toggled(bool)) , this , SIGNAL(matchRegExpToggled(bool)) );
    }

    QProgressBar* _progress = new QProgressBar(this);
    _progress->setMinimum(0);
    _progress->setMaximum(0);
    _progress->setVisible(false);

    QLabel* _continueLabel = new QLabel(this);
    _continueLabel->setVisible(false);

    layout->addWidget(close);
    layout->addWidget(findLabel);
    layout->addWidget(_searchEdit);
    layout->addWidget(findNext);
    layout->addWidget(findPrev);

    // optional features
    if ( features & HighlightMatches ) layout->addWidget(highlightMatches);
    if ( features & MatchCase        ) layout->addWidget(matchCase);
    if ( features & RegExp           ) layout->addWidget(matchRegExp);
    
    layout->addWidget(_progress);
    layout->addWidget(_continueLabel);
    layout->addStretch();

    layout->setMargin(4);

    setLayout(layout);
}

QString IncrementalSearchBar::searchText()
{
    return _searchEdit->text();
}
bool IncrementalSearchBar::highlightMatches()
{
    return _highlightMatches;
}
bool IncrementalSearchBar::matchCase()
{
    return _matchCase;
}
bool IncrementalSearchBar::matchRegExp()
{
    return _matchRegExp;
}

void IncrementalSearchBar::setFoundMatch( bool match )
{
    //FIXME - Hard coded colour used here - is there a better alternative?

    if ( !match )
    {
        _searchEdit->setStyleSheet( "QLineEdit{ background-color: #FF7777 }" );
    }
    else
    {
        _searchEdit->setStyleSheet( QString() );
    }
}

void IncrementalSearchBar::setContinueFlag( Continue flag )
{
    if ( flag == ContinueFromTop )
    {
        _continueLabel->setText( i18n("Search reached bottom, continued from top.") );
        _continueLabel->show();
    } 
    else if ( flag == ContinueFromBottom )
    {
        _continueLabel->setText( i18n("Search reached top, continued from bottom.") );
        _continueLabel->show();
    } 
    else if ( flag == ClearContinue )
    {
        _continueLabel->hide();
    }
}

#include "IncrementalSearchBar.moc"
