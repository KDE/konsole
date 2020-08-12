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

#include "EscapeSequenceUrlFilterHotSpot.h"

#include <KRun>
#include <QApplication>
#include <QMouseEvent>
#include <QDebug>

#include "widgets/TerminalDisplay.h"

using namespace Konsole;

EscapeSequenceUrlHotSpot::EscapeSequenceUrlHotSpot(
    int startLine,
    int startColumn,
    int endLine,
    int endColumn,
    const QString &text,
    const QString &url) :
    HotSpot(startLine, startColumn, endLine, endColumn),
    _text(text),
    _url(url)
{
    setType(EscapedUrl);
}

void EscapeSequenceUrlHotSpot::activate(QObject *obj)
{
    Q_UNUSED(obj)

    new KRun(QUrl(_url), QApplication::activeWindow());
}

void EscapeSequenceUrlHotSpot::mouseEnterEvent(TerminalDisplay* td, QMouseEvent* ev)
{
    auto cursor = td->cursor();
    auto r = region(td->fontWidth(), td->fontHeight(), td->columns(), td->contentRect()).first;

    td->setCursor(Qt::PointingHandCursor);
    td->update(r);
}

void EscapeSequenceUrlHotSpot::mouseReleaseEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    if ((td->openLinksByDirectClick() || ((ev->modifiers() & Qt::ControlModifier) != 0u))) {
        activate(nullptr);
    }
}
