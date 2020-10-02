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

#include "EscapeSequenceUrlFilter.h"

#include "session/Session.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "EscapeSequenceUrlExtractor.h"
#include "EscapeSequenceUrlFilterHotSpot.h"

using namespace Konsole;

EscapeSequenceUrlFilter::EscapeSequenceUrlFilter(Session* session, TerminalDisplay *window)
{
    _session = session;
    _window = window;
}

void EscapeSequenceUrlFilter::process()
{
    if (!_window->screenWindow() && _window->screenWindow()->screen()) {
        return;
    }
    auto sWindow = _window->screenWindow();
    const auto urls = sWindow->screen()->urlExtractor()->history();

    for (const auto &escapedUrl : urls) {
        if (escapedUrl.begin.row < sWindow->currentLine() || escapedUrl.end.row > sWindow->currentLine() + sWindow->windowLines()) {
            continue;
        }

        const int beginRow = escapedUrl.begin.row - sWindow->currentLine();
        const int endRow = escapedUrl.end.row - sWindow->currentLine();
        QSharedPointer<HotSpot> spot(
            // TODO:
            // This uses Column / Row while everything else uses Row/Column.
            // Move everything else to QPoint begin / QPoint End.
            new EscapeSequenceUrlHotSpot(beginRow, escapedUrl.begin.col,
                    endRow, escapedUrl.end.col,
                    escapedUrl.text,
                    escapedUrl.url
            )
        );

        addHotSpot(spot);
    }
}
