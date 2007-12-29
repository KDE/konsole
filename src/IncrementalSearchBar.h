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

#ifndef INCREMENTALSEARCHBAR_H
#define INCREMENTALSEARCHBAR_H

// Qt
#include <QtGui/QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QProgressBar;
class QTimer;

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
 * The search bar has a number of optional features which can be enabled or disabled by passing
 * a set of Features flags to the constructor.
 *
 * An optional checkbox can be displayed to indicate whether all matches in the document 
 * for searchText() should be highlighted.  
 * The highlightMatchesToggled() signal is emitted when this checkbox is toggled.
 *
 * The two further optional checkboxes allow the user to control the matching process.  
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
    enum Continue
    {
        /** 
         * Indicates that the search has reached the bottom of the document and has been continued from
         * the top 
         */
        ContinueFromTop,
        /**
         * Indicates that the search has reached the top of the document and has been continued from
         * the bottom
         */
        ContinueFromBottom,

        /** Clears the Continue flag */
        ClearContinue
    };

    /** 
     * This enum defines the features which can be supported by an implementation of
     * an incremental search bar
     */
    enum Features
    {
        /** search facility supports highlighting of all matches */
        HighlightMatches = 1,
        /** search facility supports case-sensitive and case-insensitive search */
        MatchCase        = 2,
        /** search facility supports regular expressions */
        RegExp           = 4,
        /** search facility supports all features */
        AllFeatures      = HighlightMatches | MatchCase | RegExp
    };

    /** 
     * Constructs a new incremental search bar with the given parent widget 
     * @p features specifies the features which should be made available to the user.
     */
    IncrementalSearchBar(Features features , QWidget* parent = 0);

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
    void setFoundMatch( bool match );

    /**
     * Sets a flag to indicate that the current search for matches has reached the top or bottom of
     * the document and has been continued again from the other end of the document.
     *
     * This flag will be cleared when the user presses the buttons to find a next or previous match.
     */
    void setContinueFlag( Continue flag );

    /** Returns the current search text */
    QString searchText();
    /** 
     * Returns whether matches for the current search text should be highlighted in the document. 
     * Always returns true if the highlight matches checkbox is not visible. 
     */
    bool highlightMatches();
    /** 
     * Returns whether matching for the current search text should be case sensitive.
     * Always returns false if the match case checkbox is not visible.
     */
    bool matchCase();
    /** 
     * Returns whether the current search text should be treated as plain text or a regular expression 
     * Always returns false if the match regular expression checkbox is not visible.
     */
    bool matchRegExp();

    // reimplemented
    virtual void setVisible( bool visible );
signals:
    /** Emitted when the text entered in the search box is altered */
    void searchChanged( const QString& text );
    /** Emitted when the user clicks the button to find the next match */
    void findNextClicked();
    /** Emitted when the user clicks the button to find the previous match */
    void findPreviousClicked();
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

protected:
    virtual bool eventFilter( QObject* watched , QEvent* event );

private slots:
    void notifySearchChanged();

private:
    bool _foundMatch;
    QCheckBox* _matchCaseBox;
    QCheckBox* _matchRegExpBox;
    QCheckBox* _highlightBox;

    QLineEdit* _searchEdit;
    QLabel* _continueLabel;
    QProgressBar* _progress;

    QTimer* _searchTimer;
};

}
#endif // INCREMENTALSEARCHBAR_H
