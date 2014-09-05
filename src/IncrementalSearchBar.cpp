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
#include <QLineEdit>
#include <KLocalizedString>
#include <KIcon>

using namespace Konsole;

IncrementalSearchBar::IncrementalSearchBar(QWidget* aParent)
    : QWidget(aParent)
    , _searchEdit(0)
    , _caseSensitive(0)
    , _regExpression(0)
    , _highlightMatches(0)
    , _reverseSearch(0)
    , _findNextButton(0)
    , _findPreviousButton(0)
    , _searchFromButton(0)
{
    QHBoxLayout* barLayout = new QHBoxLayout(this);

    QToolButton* closeButton = new QToolButton(this);
    closeButton->setObjectName(QLatin1String("close-button"));
    closeButton->setToolTip(i18nc("@info:tooltip", "Close the search bar"));
    closeButton->setAutoRaise(true);
    closeButton->setIcon(KIcon("dialog-close"));
    connect(closeButton , &QToolButton::clicked , this , &Konsole::IncrementalSearchBar::closeClicked);

    QLabel* findLabel = new QLabel(i18nc("@label:textbox", "Find:"), this);
    _searchEdit = new QLineEdit(this);
    _searchEdit->setClearButtonEnabled(true);
    _searchEdit->installEventFilter(this);
    _searchEdit->setObjectName(QLatin1String("search-edit"));
    _searchEdit->setToolTip(i18nc("@info:tooltip", "Enter the text to search for here"));

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
    connect(_searchTimer , &QTimer::timeout , this , &Konsole::IncrementalSearchBar::notifySearchChanged);
    connect(_searchEdit , &QLineEdit::textChanged , _searchTimer , static_cast<void(QTimer::*)()>(&QTimer::start));

    _findNextButton = new QToolButton(this);
    _findNextButton->setObjectName(QLatin1String("find-next-button"));
    _findNextButton->setText(i18nc("@action:button Go to the next phrase", "Next"));
    _findNextButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _findNextButton->setToolTip(i18nc("@info:tooltip", "Find the next match for the current search phrase"));
    connect(_findNextButton , &QToolButton::clicked , this , &Konsole::IncrementalSearchBar::findNextClicked);

    _findPreviousButton = new QToolButton(this);
    _findPreviousButton->setObjectName(QLatin1String("find-previous-button"));
    _findPreviousButton->setText(i18nc("@action:button Go to the previous phrase", "Previous"));
    _findPreviousButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    _findPreviousButton->setToolTip(i18nc("@info:tooltip", "Find the previous match for the current search phrase"));
    connect(_findPreviousButton , &QToolButton::clicked , this , &Konsole::IncrementalSearchBar::findPreviousClicked);


    _searchFromButton = new QToolButton(this);
    _searchFromButton->setObjectName(QLatin1String("search-from-button"));
    connect(_searchFromButton , &QToolButton::clicked , this , &Konsole::IncrementalSearchBar::searchFromClicked);

    QToolButton* optionsButton = new QToolButton(this);
    optionsButton->setObjectName(QLatin1String("find-options-button"));
    optionsButton->setText(i18nc("@action:button Display options menu", "Options"));
    optionsButton->setCheckable(false);
    optionsButton->setPopupMode(QToolButton::InstantPopup);
    optionsButton->setArrowType(Qt::DownArrow);
    optionsButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
    optionsButton->setToolTip(i18nc("@info:tooltip", "Display the options menu"));

    barLayout->addWidget(closeButton);
    barLayout->addWidget(findLabel);
    barLayout->addWidget(_searchEdit);
    barLayout->addWidget(_findNextButton);
    barLayout->addWidget(_findPreviousButton);
    barLayout->addWidget(_searchFromButton);
    barLayout->addWidget(optionsButton);

    // Fill the options menu
    QMenu* optionsMenu = new QMenu(this);
    optionsButton->setMenu(optionsMenu);

    _caseSensitive = optionsMenu->addAction(i18nc("@item:inmenu", "Case sensitive"));
    _caseSensitive->setCheckable(true);
    _caseSensitive->setToolTip(i18nc("@info:tooltip", "Sets whether the search is case sensitive"));
    connect(_caseSensitive, &QAction::toggled,
            this, &Konsole::IncrementalSearchBar::matchCaseToggled);

    _regExpression = optionsMenu->addAction(i18nc("@item:inmenu", "Match regular expression"));
    _regExpression->setCheckable(true);
    connect(_regExpression, &QAction::toggled,
            this, &Konsole::IncrementalSearchBar::matchRegExpToggled);

    _highlightMatches = optionsMenu->addAction(i18nc("@item:inmenu", "Highlight all matches"));
    _highlightMatches->setCheckable(true);
    _highlightMatches->setToolTip(i18nc("@info:tooltip", "Sets whether matching text should be highlighted"));
    _highlightMatches->setChecked(true);
    connect(_highlightMatches, &QAction::toggled,
            this, &Konsole::IncrementalSearchBar::highlightMatchesToggled);

    _reverseSearch = optionsMenu->addAction(i18n("Search backwards"));
    _reverseSearch->setCheckable(true);
    _reverseSearch->setToolTip(i18n("Sets whether search should start from the bottom"));
    _reverseSearch->setChecked(true);
    connect(_reverseSearch, &QAction::toggled,
            this, &Konsole::IncrementalSearchBar::updateButtonsAccordingToReverseSearchSetting);
    updateButtonsAccordingToReverseSearchSetting();

    barLayout->addStretch();

    barLayout->setContentsMargins(4, 4, 4, 4);

    setLayout(barLayout);
}

void IncrementalSearchBar::notifySearchChanged()
{
    emit searchChanged(searchText());
}

void IncrementalSearchBar::updateButtonsAccordingToReverseSearchSetting()
{
    Q_ASSERT(_reverseSearch);
    if (_reverseSearch->isChecked()) {
        _searchFromButton->setText(i18nc("@action:button Search from bottom", "From bottom"));
        _searchFromButton->setToolTip(i18n("Search for the current search phrase from the bottom"));
        _findNextButton->setIcon(KIcon("go-up-search"));
        _findPreviousButton->setIcon(KIcon("go-down-search"));
    } else {
        _searchFromButton->setText(i18nc("@action:button Search from top", "From top"));
        _searchFromButton->setToolTip(i18n("Search for the current search phrase from the top"));
        _findNextButton->setIcon(KIcon("go-down-search"));
        _findPreviousButton->setIcon(KIcon("go-up-search"));
    }
}

QString IncrementalSearchBar::searchText()
{
    return _searchEdit->text();
}

void IncrementalSearchBar::setSearchText(const QString& text)
{
    if (text != searchText())
        _searchEdit->setText(text);
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
                _findNextButton->click();
                return true;
            }

            if ((keyEvent->key() == Qt::Key_Return) && (keyEvent->modifiers() == Qt::ShiftModifier)) {
                _findPreviousButton->click();
                return true;
            }

            if ((keyEvent->key() == Qt::Key_Return) && (keyEvent->modifiers() == Qt::ControlModifier)) {
                _searchFromButton->click();
                return true;
            }
        }
    }

    return QWidget::eventFilter(watched, aEvent);
}

void IncrementalSearchBar::keyPressEvent(QKeyEvent* event)
{
    static QSet<int> movementKeysToPassAlong = QSet<int>()
        << Qt::Key_PageUp
        << Qt::Key_PageDown
        << Qt::Key_Up
        << Qt::Key_Down;

    if (movementKeysToPassAlong.contains(event->key()) &&
            (event->modifiers() == Qt::ShiftModifier)) {
        emit unhandledMovementKeyPressed(event);
    } else {
        QWidget::keyPressEvent(event);
    }
}

void IncrementalSearchBar::setVisible(bool visible)
{
    QWidget::setVisible(visible);

    if (visible) {
        focusLineEdit();
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

void IncrementalSearchBar::focusLineEdit()
{
    _searchEdit->setFocus(Qt::ActiveWindowFocusReason);
    _searchEdit->selectAll();
}

const QBitArray IncrementalSearchBar::optionsChecked()
{
    QBitArray options(4, 0);

    if (_caseSensitive->isChecked()) options.setBit(MatchCase);
    if (_regExpression->isChecked()) options.setBit(RegExp);
    if (_highlightMatches->isChecked()) options.setBit(HighlightMatches);
    if (_reverseSearch->isChecked()) options.setBit(ReverseSearch);

    return options;
}

