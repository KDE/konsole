/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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
#include <QtDebug>

// Konsole
#include "TEScreen.h"
#include "ScreenWindow.h"

ScreenWindow::ScreenWindow(QObject* parent)
    : QObject(parent)
    , _currentLine(0)
    , _trackOutput(true)
{
}

void ScreenWindow::setScreen(TEScreen* screen)
{
    Q_ASSERT( screen );

    _screen = screen;
}

TEScreen* ScreenWindow::screen() const
{
    return _screen;
}

Character* ScreenWindow::getImage()
{
    return _screen->getCookedImage(_currentLine);
}

QVector<LineProperty> ScreenWindow::getLineProperties()
{
    return _screen->getCookedLineProperties(_currentLine);
}

QString ScreenWindow::selectedText( bool preserveLineBreaks ) const
{
    return _screen->selectedText( preserveLineBreaks );
}

void ScreenWindow::setSelectionStart( int column , int line , bool columnMode )
{
    /* FIXME - I'm not sure what the columnmode parameter ( the last argument to setSelectionStart )
     does, check it out and fix */

    _screen->setSelectionStart( column , line + _currentLine  , columnMode);

    emit selectionChanged();
}

void ScreenWindow::setSelectionEnd( int column , int line )
{
    _screen->setSelectionEnd( column , line + _currentLine );

    emit selectionChanged();
}

bool ScreenWindow::isSelected( int column , int line )
{
    return _screen->isSelected( column , line + _currentLine );
}

void ScreenWindow::clearSelection()
{
    _screen->clearSelection();

    emit selectionChanged();
}

int ScreenWindow::windowLines() const
{
    return _screen->getLines();
}

int ScreenWindow::windowColumns() const
{
    return _screen->getColumns();
}

int ScreenWindow::lineCount() const
{
    return _screen->getHistLines();
}

int ScreenWindow::columnCount() const
{
    return _screen->getColumns();
}

int ScreenWindow::currentLine() const
{
    return _currentLine;
}

void ScreenWindow::scrollTo( int line )
{
    const int delta = line - _currentLine;

    _currentLine = line;

    // keep track of number of lines scrolled by,
    // this can be reset by calling resetScrollCount()
    _scrollCount += delta;
}

void ScreenWindow::setTrackOutput(bool trackOutput)
{
    _trackOutput = trackOutput;
}

bool ScreenWindow::trackOutput() const
{
    return _trackOutput;
}

int ScreenWindow::scrollCount() const
{
    return _scrollCount;
}

void ScreenWindow::resetScrollCount() 
{
    _scrollCount = 0;
}

void ScreenWindow::notifyOutputChanged()
{
    if ( _trackOutput )
    {
        scrollTo( _screen->getHistLines() );
    }

    emit outputChanged();
}

#include "ScreenWindow.moc"
