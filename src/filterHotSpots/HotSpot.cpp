/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2020 by Tomaz Canabrava <tcanabrava@gmail.com>

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

#include "HotSpot.h"

#include <QMouseEvent>
#include <QDebug>

#include "terminalDisplay/TerminalDisplay.h"
#include "FileFilterHotspot.h"

using namespace Konsole;

HotSpot::~HotSpot() = default;

HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn) :
    _startLine(startLine),
    _startColumn(startColumn),
    _endLine(endLine),
    _endColumn(endColumn),
    _type(NotSpecified)
{
}

QList<QAction *> HotSpot::actions()
{
    return {};
}

void HotSpot::setupMenu(QMenu *)
{

}

int HotSpot::startLine() const
{
    return _startLine;
}

int HotSpot::endLine() const
{
    return _endLine;
}

int HotSpot::startColumn() const
{
    return _startColumn;
}

int HotSpot::endColumn() const
{
    return _endColumn;
}

HotSpot::Type HotSpot::type() const
{
    return _type;
}

void HotSpot::setType(Type type)
{
    _type = type;
}

QPair<QRegion, QRect> HotSpot::region(int fontWidth, int fontHeight, int columns, QRect terminalDisplayRect) const
{
    QRegion region;
    QRect r;
    const int top = terminalDisplayRect.top();
    const int left = terminalDisplayRect.left();

    if (startLine() == endLine()) {
        r.setCoords(startColumn() * fontWidth + left,
                    startLine() * fontHeight + top,
                    (endColumn()) * fontWidth + left - 1,
                    (endLine() + 1) * fontHeight + top - 1);
        region |= r;
    } else {
        r.setCoords(startColumn() * fontWidth + left,
                    startLine() * fontHeight + top,
                    (columns) * fontWidth + left - 1,
                    (startLine() + 1) *fontHeight + top - 1);
        region |= r;
        for (int line = startLine() + 1 ; line < endLine() ; line++) {
            r.setCoords(0 *  fontWidth + left,
                        line *  fontHeight + top,
                        (columns)* fontWidth + left - 1,
                        (line + 1)* fontHeight + top - 1);
            region |= r;
        }
        r.setCoords(0 *  fontWidth + left,
                    endLine()* fontHeight + top,
                    (endColumn()) * fontWidth + left - 1,
                    (endLine() + 1) * fontHeight + top - 1);
        region |= r;
    }
    return {region, r};
}

void HotSpot::mouseMoveEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    Q_UNUSED(td)
    Q_UNUSED(ev)
}

bool HotSpot::isUrl()
{
    return (_type == HotSpot::Link || _type == HotSpot::EMailAddress || _type == HotSpot::EscapedUrl);
}

void HotSpot::mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    if (!isUrl()) {
        return;
    }

    if (td->cursor().shape() != Qt::PointingHandCursor
        && ((td->openLinksByDirectClick() || ((ev->modifiers() & Qt::ControlModifier) != 0u)))) {
        td->setCursor(Qt::PointingHandCursor);
    }

    auto r = region(td->fontWidth(), td->fontHeight(), td->columns(), td->contentRect()).first;
    td->update(r);
}

void HotSpot::mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    Q_UNUSED(ev)

    if (!isUrl()) {
        return;
    }

    auto r = region(td->fontWidth(), td->fontHeight(), td->columns(), td->contentRect()).first;
    td->update(r);

    td->resetCursor();
}

void HotSpot::mouseReleaseEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    if (!isUrl()) {
        return;
    }

    if ((td->openLinksByDirectClick() || ((ev->modifiers() & Qt::ControlModifier) != 0u))) {
        activate(nullptr);
    }
}

void HotSpot::keyPressEvent(TerminalDisplay* td, QKeyEvent *ev)
{
    if (!isUrl()) {
        return;
    }

    // If td->openLinksByDirectClick() is true, the shape has already been changed by the
    // mouseEnterEvent
    if (td->cursor().shape() != Qt::PointingHandCursor && (ev->modifiers() & Qt::ControlModifier) != 0u) {
        td->setCursor(Qt::PointingHandCursor);
    }
}

void HotSpot::keyReleaseEvent(TerminalDisplay *td, QKeyEvent *ev)
{
    Q_UNUSED(ev)

    if (!isUrl()) {
        return;
    }

    if (td->openLinksByDirectClick()) {
        return;
    }
    td->resetCursor();
}

void HotSpot::debug() {
    qDebug() << this;
    qDebug() <<  type();
    qDebug() << _startLine << _endLine << _startColumn << _endColumn;
}
