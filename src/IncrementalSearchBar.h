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

#ifndef INCREMENTALSEARCHBAR_H
#define INCREMENTALSEARCHBAR_H

// Qt
#include <QWidget>
#include <QtCore/QBitArray>

class QAction;
class QLabel;
class QTimer;
class QLineEdit;
class QToolButton;

namespace Konsole
{
/**
 * A widget which allows users to search incrementally through a document for a
 * a text string or regular expression.
 *
 * The widget consists of a text box into which the user can enter their search text and
 * buttons to trigger a search for the next and previous matches for the search text.
 *
 * When the search text is changed, the searchChanged() signal is emitted.  A search through
 * the document for the new text should begin immediately and the active view of the document
 * should jump to display any matches if found.  setFoundMatch() should be called whenever the
 * search text changes to indicate whether a match for the text was found in the document.
 *
 * findNextClicked() and findPreviousClicked() signals are emitted when the user presses buttons
 * to find next and previous matches respectively.
 *
 * The first indicates whether searches are case sensitive.
 * The matchCaseToggled() signal is emitted when this is changed.
 * The second indicates whether the search text should be treated as a plain string or
 * as a regular expression.
 * The matchRegExpToggled() signal is emitted when this is changed.
 */
class IncrementalSearchBar : public QWidget
{
    Q_OBJECT

public:
    /**
     * This enum defines the options that can be checked.
     */
    enum SearchOptions {
        /** Highlight all matches */
        HighlightMatches = 0,
        /** Searches are case-sensitive or not */
        MatchCase        = 1,
        /** Searches use regular expressions */
        RegExp           = 2,
        /** Search from the bottom and up **/
        ReverseSearch = 3
    };

    /**
     * Constructs a new incremental search bar with the given parent widget
     */
    explicit IncrementalSearchBar(QWidget* parent = 0);

    /* Returns search options that are checked */
    const QBitArray optionsChecked();

    /**
     * Sets an indicator for the user as to whether or not a match for the
     * current search text was found in the document.
     *
     * The indicator will not be shown if the search text is empty ( because
     * the user has not yet entered a query ).
     *
     * @param match True if a match was found or false otherwise.  If true,
     * and the search text is non-empty, an indicator that no matches were
     * found will be shown.
     */
    void setFoundMatch(bool match);

    /** Returns the current search text */
    QString searchText();

    void setSearchText(const QString& text);

    void focusLineEdit();

    // reimplemented
    virtual void setVisible(bool visible);
signals:
    /** Emitted when the text entered in the search box is altered */
    void searchChanged(const QString& text);
    /** Emitted when the user clicks the button to find the next match */
    void findNextClicked();
    /** Emitted when the user clicks the button to find the previous match */
    void findPreviousClicked();
    /** The search from beginning/end button */
    void searchFromClicked();
    /**
     * Emitted when the user toggles the checkbox to indicate whether
     * matches for the search text should be highlighted
     */
    void highlightMatchesToggled(bool);
    /**
     * Emitted when the user toggles the checkbox to indicate whether
     * matching for the search text should be case sensitive
     */
    void matchCaseToggled(bool);
    /**
     * Emitted when the user toggles the checkbox to indicate whether
     * the search text should be treated as a plain string or a regular expression
     */
    void matchRegExpToggled(bool);
    /** Emitted when the close button is clicked */
    void closeClicked();
    /** Emitted when the return button is pressed in the search box */
    void searchReturnPressed(const QString& text);
    /** Emitted when shift+return buttons are pressed in the search box */
    void searchShiftPlusReturnPressed();
    /** A movement key not handled is forwarded to the terminal display */
    void unhandledMovementKeyPressed(QKeyEvent *event);

protected:
    virtual bool eventFilter(QObject* watched , QEvent* event);
    virtual void keyPressEvent(QKeyEvent* event);

public slots:
    void clearLineEdit();

private slots:
    void notifySearchChanged();
    void updateButtonsAccordingToReverseSearchSetting();

private:
    QLineEdit* _searchEdit;
    QAction* _caseSensitive;
    QAction* _regExpression;
    QAction* _highlightMatches;
    QAction* _reverseSearch;
    QToolButton* _findNextButton;
    QToolButton* _findPreviousButton;
    QToolButton* _searchFromButton;

    QTimer* _searchTimer;
};
}
#endif // INCREMENTALSEARCHBAR_H
