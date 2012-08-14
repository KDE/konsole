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
#include <QHBoxLayout>
#include <QLabel>
#include <QtGui/QKeyEvent>
#include <QtCore/QTimer>
#include <QToolButton>
#include <QMenu>

// KDE
#include <KColorScheme>
#include <KLineEdit>
#include <KLocalizedString>
#include <KIcon>

using namespace Konsole;

IncrementalSearchBar::IncrementalSearchBar(QWidget* aParent)
    : QWidget(aParent)
    , _foundMatch(false)
    , _searchEdit(0)
    , _caseSensitive(0)
    , _regExpression(0)
    , _highlightMatches(0)
{
    QHBoxLayout* barLayout = new QHBoxLayout(this);

    QToolButton* closeButton = new QToolButton(this);
    closeButton->setObjectName(QLatin1String("close-button"));
    closeButton->setToolTip(i18n("Close the search bar"));
    closeButton->setAutoRaise(true);
    closeButton->setIcon(KIcon("dialog-close"));
    connect(closeButton , SIGNAL(clicked()) , this , SIGNAL(closeClicked()));

    QLabel* findLabel = new QLabel(i18n("Find:"), this);
    _searchEdit = new KLineEdit(this);
    _searchEdit->setClearButtonShown(true);
    _searchEdit->installEventFilter(this);
    _searchEdit->setObjectName(QLatin1String("search-edit"));
    _searchEdit->setToolTip(i18n("Enter the text to search for here"));

    // text box may be a minimum of 6 characters wide and a maximum of 10 characters wide
    // (since the maxWidth metric is used here, more characters probably will fit in than 6
    //  and 10)
    QFontMetrics metrics(_searchEdit->font());
    int maxWidth = metrics.maxWidth();
    _searchEdit->setMinimumWidth(maxWidth * 6);
    _searchEdit->setMaximumWidth(maxWidth * 10);

    _searchTimer = new QTimer(this);
    _searchTimer->setInterval(250);
    _searchTimer->setSingleShot(true);
    connect(_searchTimer , SIGNAL(timeout()) , this , SLOT(notifySearchChanged()));
    connect(_searchEdit , SIGNAL(clearButtonClicked()) , this , SLOT(clearLineEdit()));
    connect(_searchEdit , SIGNAL(textChanged(QString)) , _searchTimer , SLOT(start()));

    QToolButton* findNext = new QToolButton(this);
    findNext->setObjectName(QLatin1String("find-next-button"));
    findNext->setText(i18nc("@action:button Go to the next phrase", "Next"));
    findNext->setIcon(KIcon("go-down-search"));
    findNext->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    findNext->setToolTip(i18n("Find the next match for the current search phrase"));
    connect(findNext , SIGNAL(clicked()) , this , SIGNAL(findNextClicked()));

    QToolButton* findPrev = new QToolButton(this);
    findPrev->setObjectName(QLatin1String("find-previous-button"));
    findPrev->setText(i18nc("@action:button Go to the previous phrase", "Previous"));
    findPrev->setIcon(KIcon("go-up-search"));
    findPrev->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    findPrev->setToolTip(i18n("Find the previous match for the current search phrase"));
    connect(findPrev , SIGNAL(clicked()) , this , SIGNAL(findPreviousClicked()));

    QToolButton* optionsButton = new QToolButton(this);
    optionsButton->setObjectName(QLatin1String("find-options-button"));
    optionsButton->setText(i18nc("@action:button Display options menu", "Options"));
    optionsButton->setCheckable(false);
    optionsButton->setPopupMode(QToolButton::InstantPopup);
    optionsButton->setArrowType(Qt::DownArrow);
    optionsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    optionsButton->setToolTip(i18n("Display the options menu"));

    barLayout->addWidget(closeButton);
    barLayout->addWidget(findLabel);
    barLayout->addWidget(_searchEdit);
    barLayout->addWidget(findNext);
    barLayout->addWidget(findPrev);
    barLayout->addWidget(optionsButton);

    // Fill the options menu
    QMenu* optionsMenu = new QMenu(this);
    optionsButton->setMenu(optionsMenu);

    _caseSensitive = optionsMenu->addAction(i18n("Case sensitive"));
    _caseSensitive->setCheckable(true);
    _caseSensitive->setToolTip(i18n("Sets whether the search is case sensitive"));
    connect(_caseSensitive, SIGNAL(toggled(bool)),
            this, SIGNAL(matchCaseToggled(bool)));

    _regExpression = optionsMenu->addAction(i18n("Match regular expression"));
    _regExpression->setCheckable(true);
    connect(_regExpression, SIGNAL(toggled(bool)),
            this, SIGNAL(matchRegExpToggled(bool)));

    _highlightMatches = optionsMenu->addAction(i18n("Highlight all matches"));
    _highlightMatches->setCheckable(true);
    _highlightMatches->setToolTip(i18n("Sets whether matching text should be highlighted"));
    _highlightMatches->setChecked(true);
    connect(_highlightMatches, SIGNAL(toggled(bool)),
            this, SIGNAL(highlightMatchesToggled(bool)));

    barLayout->addStretch();

    barLayout->setContentsMargins(4, 4, 4, 4);

    setLayout(barLayout);
}
void IncrementalSearchBar::notifySearchChanged()
{
    emit searchChanged(searchText());
}
QString IncrementalSearchBar::searchText()
{
    return _searchEdit->text();
}

bool IncrementalSearchBar::eventFilter(QObject* watched , QEvent* aEvent)
{
    if (watched == _searchEdit) {
        if (aEvent->type() == QEvent::KeyPress) {
            QKeyEvent* keyEvent = static_cast<QKeyEvent*>(aEvent);

            if (keyEvent->key() == Qt::Key_Escape) {
                emit closeClicked();
                return true;
            }

            if (keyEvent->key() == Qt::Key_Return && !keyEvent->modifiers()) {
                emit searchReturnPressed(_searchEdit->text());
                return true;
            }

            if ((keyEvent->key() == Qt::Key_Return) && (keyEvent->modifiers() == Qt::ShiftModifier)) {
                emit searchShiftPlusReturnPressed();
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, aEvent);
}

void IncrementalSearchBar::setVisible(bool visible)
{
    QWidget::setVisible(visible);

    if (visible) {
        //TODO - Check if this is the correct reason value to use here
        _searchEdit->setFocus(Qt::ActiveWindowFocusReason);
        _searchEdit->selectAll();
    }
}

void IncrementalSearchBar::setFoundMatch(bool match)
{
    if (!match && !_searchEdit->text().isEmpty()) {
        KStatefulBrush backgroundBrush(KColorScheme::View, KColorScheme::NegativeBackground);

        QString matchStyleSheet = QString("QLineEdit{ background-color:%1 }")
                                  .arg(backgroundBrush.brush(_searchEdit).color().name());

        _searchEdit->setStyleSheet(matchStyleSheet);
    } else if (_searchEdit->text().isEmpty()) {
        clearLineEdit();
    } else {
        KStatefulBrush backgroundBrush(KColorScheme::View, KColorScheme::PositiveBackground);

        QString matchStyleSheet = QString("QLineEdit{ background-color:%1 }")
                                  .arg(backgroundBrush.brush(_searchEdit).color().name());

        _searchEdit->setStyleSheet(matchStyleSheet);
    }
}

void IncrementalSearchBar::clearLineEdit()
{
    _searchEdit->setStyleSheet(QString());
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
