/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.characterom>

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

#ifndef SCREENWINDOW_H
#define SCREENWINDOW_H

// Qt
#include <QObject>

class TEScreen;

/**
 * Provides a window onto a section of a terminal screen.
 * This window can then be rendered by a terminal display widget ( TEWidget ).
 *
 */
class ScreenWindow //: public QObject
{
public:
    ScreenWindow();

    /** Sets the screen which this window looks onto */
    void setScreen(TEScreen* screen);
    /** Returns the screen which this window looks onto */
    TEScreen* screen() const;

    /** 
     * Returns the image of characters which are currently visible through this window
     * onto the screen.
     *
     * getImage() creates a new buffer consisting of lines() * columns() characters and
     * copies the characters from the appropriate part of the screen into the buffer.
     * It is the caller's responsibility to free the buffer when they have finished using
     * it.
     */
    Character* getImage();
    /**
     * Returns the line attributes associated with the lines of characters which
     * are currently visible through this window
     */
    QVector<LineProperty> getLineProperties();

    /**
     * Returns the number of lines which the window has been scrolled by since the last
     * call to resetScrollCount().
     * This allows views to optimise scrolling operations.     
     */
    int scrollCount() const;

    /**
     * Resets the count of scrolled lines returned by scrollCount()
     */
    void resetScrollCount();

    /** 
     * Sets the start of the selection to the given @p line and @p column within 
     * the window.
     */
    void setSelectionStart( int column , int line , bool columnMode );
    /**
     * Sets the end of the selection to the given @p line and @p column within
     * the window.
     */
    void setSelectionEnd( int column , int line ); 
    /**
     * Returns true if the character at @p line , @p column is part of the selection.
     */
    bool isSelected( int column , int line );
    /** 
     * Clears the current selection
     */
    void clearSelection();

    /** Returns the number of lines in the window */
    int windowLines() const;
    /** Returns the number of columns in the window */
    int windowColumns() const;
    
    /** Returns the total number of lines in the screen */
    int lineCount() const;
    /** Returns the total number of columns in the screen */
    int columnCount() const;

    /** Returns the index of the line which is currently at the top of this window */
    int currentLine() const;

    /** Scrolls the window so that @p line is at the top of the window */
    void scrollTo( int line );

    /** 
     * Specifies whether the window should automatically move to the bottom
     * of the screen when new output is added.
     */
    void setTrackOutput(bool trackOutput);
    /** 
     * Returns whether the window automatically moves to the bottom of the screen as
     * new output is added
     */
    bool trackOutput() const;

    /**
     * Returns the text which is currently selected.
     *
     * @param preserveLineBreaks TODO: Document me
     */
    QString selectedText( bool preserveLineBreaks ) const;

//signals:
    /**
     * Emitted when the number of lines or columns in the associated screen changes
     */
  //  void screenSizeChanged();

private:
    TEScreen* _screen; // see setScreen() , screen()

    int  _currentLine; // see scrollTo() , currentLine()
    bool _trackOutput; // see setTrackOutput() , trackOutput() 
    int  _scrollCount; // count of lines which the window has been scrolled by since
                       // the last call to resetScrollCount()
};

#endif // SCREENWINDOW_H
