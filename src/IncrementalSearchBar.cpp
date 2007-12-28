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

// Own
#include "IncrementalSearchBar.h"

// Qt
#include <QtGui/QCheckBox>
#include <QtGui/QBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>
#include <QtGui/QProgressBar>
#include <QtGui/QKeyEvent>
#include <QtCore/QTimer>
#include <QtGui/QToolButton>

// KDE
#include <KColorScheme>
#include <KLocale>
#include <KIcon>

using namespace Konsole;

IncrementalSearchBar::IncrementalSearchBar(Features features , QWidget* parent)
    : QWidget(parent)
    , _foundMatch(false)
    , _matchCaseBox(0)
    , _matchRegExpBox(0)
    , _highlightBox(0)
    , _searchEdit(0)
    , _continueLabel(0)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
  
    QToolButton* close = new QToolButton(this);
    close->setObjectName("close-button");
    close->setToolTip( i18n("Close the search bar") );
    close->setAutoRaise(true);
    close->setIcon(KIcon("dialog-close"));
    connect( close , SIGNAL(clicked()) , this , SIGNAL(closeClicked()) );

    QLabel* findLabel = new QLabel(i18n("Find:"),this);
    _searchEdit = new QLineEdit(this);
    _searchEdit->installEventFilter(this);
    _searchEdit->setObjectName("search-edit");
    _searchEdit->setToolTip( i18n("Enter the text to search for here") );

    // text box may be a minimum of 6 characters wide and a maximum of 10 characters wide
    // (since the maxWidth metric is used here, more characters probably will fit in than 6
    //  and 10)
    QFontMetrics metrics(_searchEdit->font());
    int maxWidth = metrics.maxWidth();
    _searchEdit->setMinimumWidth(maxWidth*6);
    _searchEdit->setMaximumWidth(maxWidth*10);

    _searchTimer = new QTimer(this);
    _searchTimer->setInterval(250);
    _searchTimer->setSingleShot(true);
    connect( _searchTimer , SIGNAL(timeout()) , this , SLOT(notifySearchChanged()) );
    connect( _searchEdit , SIGNAL(textChanged(const QString&)) , _searchTimer , SLOT(start()));

    QToolButton* findNext = new QToolButton(this);
    findNext->setObjectName("find-next-button");
    findNext->setText(i18n("Next"));
    findNext->setAutoRaise(true);
    findNext->setIcon( KIcon("go-down-search") );
    findNext->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    findNext->setToolTip( i18n("Find the next match for the current search phrase") );
    connect( findNext , SIGNAL(clicked()) , this , SIGNAL(findNextClicked()) );

    QToolButton* findPrev = new QToolButton(this);
    findPrev->setObjectName("find-previous-button");
    findPrev->setText(i18n("Previous"));
    findPrev->setAutoRaise(true);
    findPrev->setIcon( KIcon("go-up-search") );
    findPrev->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    findPrev->setToolTip( i18n("Find the previous match for the current search phrase") );
    connect( findPrev , SIGNAL(clicked()) , this , SIGNAL(findPreviousClicked()) );

    if ( features & HighlightMatches )
    {
        _highlightBox = new QCheckBox( i18n("Highlight all") , this );
        _highlightBox->setObjectName("highlight-matches-box");
        _highlightBox->setToolTip( i18n("Sets whether matching text should be highlighted") );
        _highlightBox->setChecked(true);
        connect( _highlightBox , SIGNAL(toggled(bool)) , this , 
                 SIGNAL(highlightMatchesToggled(bool)) );
    }

    if ( features & MatchCase )
    {
        _matchCaseBox = new QCheckBox( i18n("Match case") , this );
        _matchCaseBox->setObjectName("match-case-box");
        _matchCaseBox->setToolTip( i18n("Sets whether the searching is case sensitive") );
        connect( _matchCaseBox , SIGNAL(toggled(bool)) , this , SIGNAL(matchCaseToggled(bool)) );
    }

    if ( features & RegExp )
    {
        _matchRegExpBox = new QCheckBox( i18n("Match regular expression") , this );
        _matchRegExpBox->setObjectName("match-regexp-box");
        _matchRegExpBox->setToolTip( i18n("Sets whether the search phrase is interpreted as normal text or"
					  " as a regular expression") );
        connect( _matchRegExpBox , SIGNAL(toggled(bool)) , this , SIGNAL(matchRegExpToggled(bool)) );
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
    if ( features & HighlightMatches ) layout->addWidget(_highlightBox);
    if ( features & MatchCase        ) layout->addWidget(_matchCaseBox);
    if ( features & RegExp           ) layout->addWidget(_matchRegExpBox);
    
    layout->addWidget(_progress);
    layout->addWidget(_continueLabel);
    layout->addStretch();

    layout->setMargin(4);

    setLayout(layout);
}
void IncrementalSearchBar::notifySearchChanged()
{
    emit searchChanged( searchText() );
}
QString IncrementalSearchBar::searchText()
{
    return _searchEdit->text();
}
bool IncrementalSearchBar::highlightMatches()
{
    if ( !_highlightBox )
    {
        return true;
    }
    else
    {
        return _highlightBox->isChecked();
    }
}
bool IncrementalSearchBar::matchCase()
{
    if ( !_matchCaseBox )
    {
        return false;
    }
    else
    {
        return _matchCaseBox->isChecked();
    }
}
bool IncrementalSearchBar::matchRegExp()
{
    if ( !_matchRegExpBox )
    {
        return false;
    }
    else
    {
        return _matchRegExpBox->isChecked();
    }
}

bool IncrementalSearchBar::eventFilter(QObject* watched , QEvent* event)
{
    if ( watched == _searchEdit )
    {
        if ( event->type() == QEvent::KeyPress )
        {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);

            if ( keyEvent->key() == Qt::Key_Escape )
            {
                emit closeClicked();
                return true;
            }
        }        
    }
        
    return QWidget::eventFilter(watched,event);
}

void IncrementalSearchBar::setVisible(bool visible)
{
    QWidget::setVisible(visible);

    if ( visible )
    {
        //TODO - Check if this is the correct reason value to use here
        _searchEdit->setFocus( Qt::ActiveWindowFocusReason );
        _searchEdit->selectAll();
    }
}

void IncrementalSearchBar::setFoundMatch( bool match )
{
    if ( !match && !_searchEdit->text().isEmpty() )
    {
		KStatefulBrush foregroundBrush(KColorScheme::View,KColorScheme::NegativeText);
		KStatefulBrush backgroundBrush(KColorScheme::View,KColorScheme::NegativeBackground);

		QString styleSheet = QString("QLineEdit{ color:%1 ; background-color:%2 }")
							 .arg(foregroundBrush.brush(_searchEdit).color().name())
							 .arg(backgroundBrush.brush(_searchEdit).color().name());

        _searchEdit->setStyleSheet( styleSheet );
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
