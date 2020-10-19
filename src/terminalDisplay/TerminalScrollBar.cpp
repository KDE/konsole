/*
    Copyright 2020-2020 by Gustavo Carneiro <gcarneiroa@hotmail.com>
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 1997,1998 by Lars Doelle <lars.doelle@on-line.de>

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
#include "TerminalScrollBar.hpp"

// Konsole
#include "TerminalDisplay.h"
#include "session/SessionController.h"
#include "../characters/Character.h"

// KDE
#include <KMessageWidget>

// Qt
#include <QRect>
#include <QTimer>
#include <QLabel>

namespace Konsole
{
    TerminalScrollBar::TerminalScrollBar(TerminalDisplay *display)
        : QScrollBar(display)
        , _display(display)
        , _scrollFullPage(false)
        , _scrollbarLocation(Enum::ScrollBarRight)
    {
        connect(this, &QScrollBar::valueChanged, this, &TerminalScrollBar::scrollBarPositionChanged);
    }

    void TerminalScrollBar::setScrollBarPosition(Enum::ScrollBarPositionEnum position)
    {
        if (_scrollbarLocation == position) {
            return;
        }

        _scrollbarLocation = position;
        applyScrollBarPosition(true);
    }
    
    void TerminalScrollBar::setScroll(int cursor, int slines) 
    {
        // update _scrollBar if the range or value has changed,
        // otherwise return
        //
        // setting the range or value of a _scrollBar will always trigger
        // a repaint, so it should be avoided if it is not necessary
        if (this->minimum() == 0                 &&
                this->maximum() == (slines - _display->_lines) &&
                this->value()   == cursor) {
            return;
        }

        disconnect(this, &QScrollBar::valueChanged, this, &TerminalScrollBar::scrollBarPositionChanged);
        setRange(0, slines - _display->_lines);
        setSingleStep(1);
        setPageStep(_display->_lines);
        setValue(cursor);
        connect(this, &QScrollBar::valueChanged, this, &TerminalScrollBar::scrollBarPositionChanged);
    }
    
    void TerminalScrollBar::setScrollFullPage(bool fullPage) 
    {
        _scrollFullPage = fullPage;
    }
    
    bool TerminalScrollBar::scrollFullPage() const
    {
        return _scrollFullPage;
    }
    
    void TerminalScrollBar::setHighlightScrolledLines(bool highlight) 
    {
        _display->_highlightScrolledLinesControl.enabled = highlight;

        if (_display->_highlightScrolledLinesControl.enabled && _display->_highlightScrolledLinesControl.timer == nullptr) {
            // setup timer for diming the highlight on scrolled lines
            _display->_highlightScrolledLinesControl.timer = new QTimer(this);
            _display->_highlightScrolledLinesControl.timer->setSingleShot(true);
            _display->_highlightScrolledLinesControl.timer->setInterval(250);
            connect(_display->_highlightScrolledLinesControl.timer, &QTimer::timeout, this, &TerminalScrollBar::highlightScrolledLinesEvent);
        }
    }
    
    bool TerminalScrollBar::alternateScrolling() const
    {
        return _alternateScrolling;
    }
    
    void TerminalScrollBar::setAlternateScrolling(bool enable) 
    {
        _alternateScrolling = enable;
    }
    
    void TerminalScrollBar::scrollBarPositionChanged(int) 
    {
        if (_display->screenWindow().isNull()) {
            return;
        }

        _display->screenWindow()->scrollTo(this->value());

        // if the thumb has been moved to the bottom of the _scrollBar then set
        // the display to automatically track new output,
        // that is, scroll down automatically
        // to how new _lines as they are added
        const bool atEndOfOutput = (this->value() == this->maximum());
        _display->screenWindow()->setTrackOutput(atEndOfOutput);

        _display->updateImage();
    }
    
    void TerminalScrollBar::highlightScrolledLinesEvent() 
    {
        _display->update(_display->_highlightScrolledLinesControl.rect);
    }
    
    void TerminalScrollBar::applyScrollBarPosition(bool propagate) 
    {
        setHidden(_scrollbarLocation == Enum::ScrollBarHidden);

        if (propagate) {
            _display->propagateSize();
            _display->update();
        }
    }
    
    // scrolls the image by 'lines', down if lines > 0 or up otherwise.
    //
    // the terminal emulation keeps track of the scrolling of the character
    // image as it receives input, and when the view is updated, it calls scrollImage()
    // with the final scroll amount.  this improves performance because scrolling the
    // display is much cheaper than re-rendering all the text for the
    // part of the image which has moved up or down.
    // Instead only new lines have to be drawn
    void TerminalScrollBar::scrollImage(int lines, const QRect &screenWindowRegion) 
    {
        // return if there is nothing to do
        if ((lines == 0) || (_display->_image == nullptr)) {
            return;
        }

        // if the flow control warning is enabled this will interfere with the
        // scrolling optimizations and cause artifacts.  the simple solution here
        // is to just disable the optimization whilst it is visible
        if ((_display->_outputSuspendedMessageWidget != nullptr) && _display->_outputSuspendedMessageWidget->isVisible()) {
            return;
        }

        if ((_display->_readOnlyMessageWidget != nullptr) && _display->_readOnlyMessageWidget->isVisible()) {
            return;
        }

        // constrain the region to the display
        // the bottom of the region is capped to the number of lines in the display's
        // internal image - 2, so that the height of 'region' is strictly less
        // than the height of the internal image.
        QRect region = screenWindowRegion;
        region.setBottom(qMin(region.bottom(), _display->_lines - 2));

        // return if there is nothing to do
        if (!region.isValid()
                || (region.top() + abs(lines)) >= region.bottom()
                || _display->_lines <= region.bottom()) {
            return;
        }

        // hide terminal size label to prevent it being scrolled
        if ((_display->_resizeWidget != nullptr) && _display->_resizeWidget->isVisible()) {
            _display->_resizeWidget->hide();
        }

        // Note:  With Qt 4.4 the left edge of the scrolled area must be at 0
        // to get the correct (newly exposed) part of the widget repainted.
        //
        // The right edge must be before the left edge of the scroll bar to
        // avoid triggering a repaint of the entire widget, the distance is
        // given by SCROLLBAR_CONTENT_GAP
        //
        // Set the QT_FLUSH_PAINT environment variable to '1' before starting the
        // application to monitor repainting.
        //
        const int scrollBarWidth = this->isHidden() ? 0 : this->width();
        const int SCROLLBAR_CONTENT_GAP = 1;
        QRect scrollRect;
        if (_scrollbarLocation == Enum::ScrollBarLeft) {
            scrollRect.setLeft(scrollBarWidth + SCROLLBAR_CONTENT_GAP + (_display->_highlightScrolledLinesControl.enabled ?
                                                                            _display->HIGHLIGHT_SCROLLED_LINES_WIDTH : 0));
            scrollRect.setRight(_display->width());
        } else {
            scrollRect.setLeft(_display->_highlightScrolledLinesControl.enabled ?
                                            _display->HIGHLIGHT_SCROLLED_LINES_WIDTH : 0);
            
            scrollRect.setRight(_display->width() - scrollBarWidth - SCROLLBAR_CONTENT_GAP);
        }
        void *firstCharPos = &_display->_image[region.top() * _display->_columns];
        void *lastCharPos = &_display->_image[(region.top() + abs(lines)) * _display->_columns];

        const int top = _display->_contentRect.top() + (region.top() * _display->_fontHeight);
        const int linesToMove = region.height() - abs(lines);
        const int bytesToMove = linesToMove * _display->_columns * sizeof(Character);

        Q_ASSERT(linesToMove > 0);
        Q_ASSERT(bytesToMove > 0);

        scrollRect.setTop( lines > 0 ? top : top + abs(lines) * _display->_fontHeight);
        scrollRect.setHeight(linesToMove * _display->_fontHeight);

        if (!scrollRect.isValid() || scrollRect.isEmpty()) {
            return;
        }

        // scroll internal image
        if (lines > 0) {
            // check that the memory areas that we are going to move are valid
            Q_ASSERT((char*)lastCharPos + bytesToMove <
                    (char*)(_display->_image + (_display->_lines * _display->_columns)));
            Q_ASSERT((lines * _display->_columns) < _display->_imageSize);
            
            // scroll internal image down
            memmove(firstCharPos, lastCharPos, bytesToMove);
        } else {
            // check that the memory areas that we are going to move are valid
            Q_ASSERT((char*)firstCharPos + bytesToMove <
                    (char*)(_display->_image + (_display->_lines * _display->_columns)));

            //scroll internal image up
            memmove(lastCharPos , firstCharPos , bytesToMove);
        }

        // scroll the display vertically to match internal _image
        _display->scroll(0, _display->_fontHeight * (-lines), scrollRect);
    }
}
