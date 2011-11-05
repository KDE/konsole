/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>

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
#include <QtGui/QKeyEvent>
#include <QtCore/QTimer>
#include <QtGui/QToolButton>
#include <QtGui/QMenu>

// KDE
#include <KColorScheme>
#include <KLineEdit>
#include <KLocale>
#include <KIcon>

using namespace Konsole;

IncrementalSearchBar::IncrementalSearchBar(QWidget* parent)
    : QWidget(parent)
    , _foundMatch(false)
    , _searchEdit(0)
    ,_caseSensitive(0)
    ,_regExpression(0)
    ,_highlightMatches(0)
{
    QHBoxLayout* layout = new QHBoxLayout(this);

    QToolButton* close = new QToolButton(this);
    close->setObjectName( QLatin1String("close-button" ));
    close->setToolTip( i18n("Close the search bar") );
    close->setAutoRaise(true);
    close->setIcon(KIcon("dialog-close"));
    connect( close , SIGNAL(clicked()) , this , SIGNAL(closeClicked()) );

    QLabel* findLabel = new QLabel(i18n("Find:"),this);
    _searchEdit = new KLineEdit(this);
    _searchEdit->setClearButtonShown(true);
    _searchEdit->installEventFilter(this);
    _searchEdit->setObjectName( QLatin1String("search-edit" ));
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
    connect( _searchEdit , SIGNAL(clearButtonClicked()) , this , SLOT(clearLineEdit()) );
    connect( _searchEdit , SIGNAL(textChanged(QString)) , _searchTimer , SLOT(start()));

    QToolButton* findNext = new QToolButton(this);
    findNext->setObjectName( QLatin1String("find-next-button" ));
    findNext->setText(i18nc("@action:button Go to the next phrase", "Next"));
    findNext->setIcon( KIcon("go-down-search") );
    findNext->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    findNext->setToolTip( i18n("Find the next match for the current search phrase") );
    connect( findNext , SIGNAL(clicked()) , this , SIGNAL(findNextClicked()) );

    QToolButton* findPrev = new QToolButton(this);
    findPrev->setObjectName( QLatin1String("find-previous-button" ));
    findPrev->setText(i18nc("@action:button Go to the previous phrase", "Previous"));
    findPrev->setIcon( KIcon("go-up-search") );
    findPrev->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    findPrev->setToolTip( i18n("Find the previous match for the current search phrase") );
    connect( findPrev , SIGNAL(clicked()) , this , SIGNAL(findPreviousClicked()) );

    QToolButton* optionsButton = new QToolButton(this);
    optionsButton->setObjectName( QLatin1String("find-options-button" ));
    optionsButton->setText(i18nc("@action:button Display options menu", "Options"));
    optionsButton->setCheckable(false);
    optionsButton->setPopupMode(QToolButton::InstantPopup);
    optionsButton->setArrowType(Qt::DownArrow);
    optionsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    optionsButton->setToolTip( i18n("Display the options menu") );

    layout->addWidget(close);
    layout->addWidget(findLabel);
    layout->addWidget(_searchEdit);
    layout->addWidget(findNext);
    layout->addWidget(findPrev);
    layout->addWidget(optionsButton);

    // Fill the options menu
    QMenu* optionsMenu = new QMenu(this);
    optionsButton->setMenu(optionsMenu);

    _caseSensitive = optionsMenu->addAction(i18n("Case sensitive"));
    _caseSensitive->setCheckable(true);
    _caseSensitive->setToolTip(i18n("Sets whether the search is case sensitive"));
    connect(_caseSensitive, SIGNAL(toggled(bool)),
            this, SIGNAL(matchCaseToggled(bool)) );

    _regExpression = optionsMenu->addAction(i18n("Match regular expression"));
    _regExpression->setCheckable(true);
    connect(_regExpression, SIGNAL(toggled(bool)),
            this, SIGNAL(matchRegExpToggled(bool)));

    _highlightMatches = optionsMenu->addAction(i18n("Highlight all matches"));
    _highlightMatches->setCheckable(true);
    _highlightMatches->setToolTip(i18n("Sets whether matching text should be highlighted"));
    _highlightMatches->setChecked(true);
    connect(_highlightMatches, SIGNAL(toggled(bool)),
            this, SIGNAL(highlightMatchesToggled(bool)) );

    layout->addStretch();

    layout->setContentsMargins(4, 4, 4, 4);

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
        KStatefulBrush backgroundBrush(KColorScheme::View,KColorScheme::NegativeBackground);

        QString styleSheet = QString("QLineEdit{ background-color:%1 }")
                             .arg(backgroundBrush.brush(_searchEdit).color().name());

        _searchEdit->setStyleSheet( styleSheet );
    }
    else
    {
        _searchEdit->setStyleSheet( QString() );
    }
}

void IncrementalSearchBar::clearLineEdit()
{
    _searchEdit->setStyleSheet( QString() );
}

const QBitArray IncrementalSearchBar::optionsChecked()
{
    QBitArray options(3, 0);

    if (_caseSensitive->isChecked()) options.setBit(MatchCase);
    if (_regExpression->isChecked()) options.setBit(RegExp);
    if (_highlightMatches->isChecked()) options.setBit(HighlightMatches);

    return options;
}


#include "IncrementalSearchBar.moc"
