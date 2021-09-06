/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "EscapeSequenceUrlFilter.h"

#include "EscapeSequenceUrlExtractor.h"
#include "EscapeSequenceUrlFilterHotSpot.h"
#include "session/Session.h"
#include "terminalDisplay/TerminalDisplay.h"

using namespace Konsole;

EscapeSequenceUrlFilter::EscapeSequenceUrlFilter(Session *session, TerminalDisplay *window)
{
    _session = session;
    _window = window;
}

void EscapeSequenceUrlFilter::process()
{
    if ((_window->screenWindow() == nullptr) && (_window->screenWindow()->screen() != nullptr)) {
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
            new EscapeSequenceUrlHotSpot(beginRow, escapedUrl.begin.col, endRow, escapedUrl.end.col, escapedUrl.text, escapedUrl.url));

        addHotSpot(spot);
    }
}
