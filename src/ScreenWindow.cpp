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

// Own
#include "ScreenWindow.h"

// Qt
#include <KDebug>

// Konsole
#include "Screen.h"

using namespace Konsole;

ScreenWindow::ScreenWindow(QObject* parent)
    : QObject(parent)
	, _windowBuffer(0)
	, _windowBufferSize(0)
	, _bufferNeedsUpdate(true)
	, _windowLines(1)
    , _currentLine(0)
    , _trackOutput(true)
    , _scrollCount(0)
{
}
ScreenWindow::~ScreenWindow()
{
	delete[] _windowBuffer;
}
void ScreenWindow::setScreen(Screen* screen)
{
    Q_ASSERT( screen );

    _screen = screen;
}

Screen* ScreenWindow::screen() const
{
    return _screen;
}

Character* ScreenWindow::getImage()
{
	// reallocate internal buffer if the window size has changed
	int size = windowLines() * windowColumns();
	if (_windowBuffer == 0 || _windowBufferSize != size) 
	{
		delete[] _windowBuffer;
		_windowBufferSize = size;
		_windowBuffer = new Character[size];
		_bufferNeedsUpdate = true;
	}

	 if (!_bufferNeedsUpdate)
		return _windowBuffer;
 
	_screen->getImage(_windowBuffer,size,
					  _currentLine,endWindowLine());

	// this window may look beyond the end of the screen, in which 
	// case there will be an unused area which needs to be filled
	// with blank characters
	fillUnusedArea();

	_bufferNeedsUpdate = false;
	return _windowBuffer;
}

void ScreenWindow::fillUnusedArea()
{
	int screenEndLine = _screen->getHistLines() + _screen->getLines() - 1;
	int windowEndLine = _currentLine + windowLines() - 1;

	int unusedLines = windowEndLine - screenEndLine;
	int charsToFill = unusedLines * windowColumns();

	Screen::fillWithDefaultChar(_windowBuffer + _windowBufferSize - charsToFill,charsToFill); 
}

// return the index of the line at the end of this window, or if this window 
// goes beyond the end of the screen, the index of the line at the end
// of the screen.
//
// when passing a line number to a Screen method, the line number should
// never be more than endWindowLine()
//
int ScreenWindow::endWindowLine() const
{
	return qMin(_currentLine + windowLines() - 1,
				_screen->getHistLines() + _screen->getLines() - 1);
}
QVector<LineProperty> ScreenWindow::getLineProperties()
{
    QVector<LineProperty> result = _screen->getLineProperties(_currentLine,endWindowLine());
	
	if (result.count() != windowLines())
		result.resize(windowLines());

	return result;
}

QString ScreenWindow::selectedText( bool preserveLineBreaks ) const
{
    return _screen->selectedText( preserveLineBreaks );
}

void ScreenWindow::getSelectionStart( int& column , int& line )
{
    _screen->getSelectionStart(column,line);
    line -= _currentLine;
}
void ScreenWindow::getSelectionEnd( int& column , int& line )
{
    _screen->getSelectionEnd(column,line);
    line -= _currentLine;
}
void ScreenWindow::setSelectionStart( int column , int line , bool columnMode )
{
	#warning "FIXME: The columnMode parameter is handled correctly when visually selecting an area, but copy/select and paste produces the wrong results."

    _screen->setSelectionStart( column , qMin(line + _currentLine,endWindowLine())  , columnMode);
	
	_bufferNeedsUpdate = true;
    emit selectionChanged();
}

void ScreenWindow::setSelectionEnd( int column , int line )
{
    _screen->setSelectionEnd( column , qMin(line + _currentLine,endWindowLine()) );

	_bufferNeedsUpdate = true;
    emit selectionChanged();
}

bool ScreenWindow::isSelected( int column , int line )
{
    return _screen->isSelected( column , qMin(line + _currentLine,endWindowLine()) );
}

void ScreenWindow::clearSelection()
{
    _screen->clearSelection();

    emit selectionChanged();
}

void ScreenWindow::setWindowLines(int lines)
{
	Q_ASSERT(lines > 0);
	_windowLines = lines;
}
int ScreenWindow::windowLines() const
{
	return _windowLines;		
}

int ScreenWindow::windowColumns() const
{
    return _screen->getColumns();
}

int ScreenWindow::lineCount() const
{
    return _screen->getHistLines() + _screen->getLines();
}

int ScreenWindow::columnCount() const
{
    return _screen->getColumns();
}

QPoint ScreenWindow::cursorPosition() const
{
    QPoint position;
    
    position.setX( _screen->getCursorX() );
    position.setY( _screen->getCursorY() );

    return position; 
}

int ScreenWindow::currentLine() const
{
    return _currentLine;
}

void ScreenWindow::scrollBy( RelativeScrollMode mode , int amount )
{
    if ( mode == ScrollLines )
    {
        scrollTo( currentLine() + amount );
    }
    else if ( mode == ScrollPages )
    {
        scrollTo( currentLine() + amount * ( windowLines() / 2 ) ); 
    }
}

bool ScreenWindow::atEndOfOutput() const
{
    return _currentLine >= (lineCount()-windowLines());
}

void ScreenWindow::scrollTo( int line )
{
    //kDebug() << "ScreenWindow scrolled to " << line << ":" << this;
    if ( line < 0 )
       line = 0;

    if ( (lineCount() - windowLines()) < line )
       line = qMax(0,lineCount() - windowLines()); 

    const int delta = line - _currentLine;

    _currentLine = line;

    // keep track of number of lines scrolled by,
    // this can be reset by calling resetScrollCount()
    _scrollCount += delta;

	_bufferNeedsUpdate = true;

    emit scrolled(_currentLine);
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
   // kDebug() << "window returning scroll count of " << _scrollCount;

    return _scrollCount;
}

void ScreenWindow::resetScrollCount() 
{
    _scrollCount = 0;
}

QRect ScreenWindow::scrollRegion() const
{
    QRect rect = _screen->lastScrolledRegion();

    // bound scroll region size to size of window
    //rect.setHeight( qMin(rect.height() , windowLines()+1) );
   
    //Q_ASSERT( rect.top() >= 0 && rect.left() >= 0 ); 
    //Q_ASSERT( rect.height()-1 <= windowLines() );

    return rect;
}

void ScreenWindow::notifyOutputChanged()
{
    // move window to the bottom of the screen and update scroll count
    // if this window is currently tracking the bottom of the screen
    if ( _trackOutput )
    { 
        _scrollCount -= _screen->scrolledLines();
        _currentLine = qMax(0,_screen->getHistLines() - (windowLines()-_screen->getLines()));
    }
    else
    {
        // if the history is not unlimited then it may 
        // have run out of space and dropped the oldest
        // lines of output - in this case the screen
        // window's current line number will need to 
        // be adjusted - otherwise the output will scroll
        _currentLine = qMax(0,_currentLine - 
                              _screen->droppedLines());

        // ensure that the screen window's current position does
        // not go beyond the bottom of the screen
        _currentLine = qMin( _currentLine , _screen->getHistLines() );
    }

	_bufferNeedsUpdate = true;

    emit outputChanged(); 
}

#include "ScreenWindow.moc"
