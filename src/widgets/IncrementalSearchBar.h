/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef INCREMENTALSEARCHBAR_H
#define INCREMENTALSEARCHBAR_H

// Qt
#include <QBitArray>
#include <QWidget>

class QAction;
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
        MatchCase = 1,
        /** Searches use regular expressions */
        RegExp = 2,
        /** Search from the bottom and up **/
        ReverseSearch = 3,
    };

    /**
     * Constructs a new incremental search bar with the given parent widget
     */
    explicit IncrementalSearchBar(QWidget *parent = nullptr);

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

    void setSearchText(const QString &text);

    void focusLineEdit();

    void setOptions();

    // reimplemented
    void setVisible(bool visible) override;
Q_SIGNALS:
    /** Emitted when the text entered in the search box is altered */
    void searchChanged(const QString &text);
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
     * the search direction should be reversed.
     */
    void reverseSearchToggled(bool);
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
    void searchReturnPressed(const QString &text);
    /** Emitted when shift+return buttons are pressed in the search box */
    void searchShiftPlusReturnPressed();
    /** A movement key not handled is forwarded to the terminal display */
    void unhandledMovementKeyPressed(QKeyEvent *event);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
public Q_SLOTS:
    void clearLineEdit();

private Q_SLOTS:
    void notifySearchChanged();
    void updateButtonsAccordingToReverseSearchSetting();

private:
    Q_DISABLE_COPY(IncrementalSearchBar)

    QLineEdit *_searchEdit;
    QAction *_caseSensitive;
    QAction *_regExpression;
    QAction *_highlightMatches;
    QAction *_reverseSearch;
    QToolButton *_findNextButton;
    QToolButton *_findPreviousButton;
    QToolButton *_searchFromButton;
    QTimer *_searchTimer;
};
}
#endif // INCREMENTALSEARCHBAR_H
