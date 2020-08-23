/*
    This file is part of Konsole, KDE's terminal.

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

#include "EscapeSequenceUrlExtractor.h"

#include <QUrl>

namespace Konsole {
EscapeSequenceUrlExtractor::EscapeSequenceUrlExtractor()
{
}

void Konsole::EscapeSequenceUrlExtractor::setScreen(Konsole::Screen* screen)
{
    _screen = screen;
    clear();
}

bool EscapeSequenceUrlExtractor::reading() const
{
    return _reading;
}

void EscapeSequenceUrlExtractor::beginUrlInput()
{
    _reading = true;
}

void EscapeSequenceUrlExtractor::appendUrlText(QChar c)
{
    if (!reading()) {
        return;
    }

    if (_currentUrl.text.isEmpty()) {
        // We need to  on getCursorX because we want the position of the
        // last printed character, not the cursor.
        const int realCcolumn = _screen->getCursorY() + _screen->getHistLines();
        _currentUrl.begin = Coordinate{realCcolumn, _screen->getCursorX() - 1};
    }
    _currentUrl.text += c;
}

void EscapeSequenceUrlExtractor::setUrl(const QString& url)
{
    if (_allowedUriSchemas.contains(QUrl(url).scheme() + QLatin1String("://"))) {
        _currentUrl.url = url;
    } else {
        abortUrlInput();
    }
}

void EscapeSequenceUrlExtractor::abortUrlInput()
{
    _reading = false;
    _currentUrl = ExtractedUrl{};
    _ignoreNextUrlInput = true;
}

void EscapeSequenceUrlExtractor::endUrlInput()
{
    Q_ASSERT(reading());
    _reading = false;

    const int realCcolumn = _screen->getCursorY() + _screen->getHistLines();
    const auto currentPos = Coordinate{realCcolumn, _screen->getCursorX()};
    _currentUrl.end = currentPos;
    _history.append(_currentUrl);

    _currentUrl = ExtractedUrl{};
}

void EscapeSequenceUrlExtractor::clear()
{
    _history.clear();
}

void EscapeSequenceUrlExtractor::setAllowedLinkSchema(const QStringList& schema)
{
    _allowedUriSchemas = schema;
}

void EscapeSequenceUrlExtractor::historyLinesRemoved(int lines)
{
    for (auto &url : _history) {
        url.begin.row -= lines;
        url.end.row -= lines;
   }
    _history.erase(
        std::remove_if(std::begin(_history), std::end(_history), [](const ExtractedUrl& url) {
            const bool toRemove = url.begin.row < 0;
            return toRemove;

        }),
        std::end(_history));
}

QVector<ExtractedUrl> EscapeSequenceUrlExtractor::history() const
{
    return _history;
}

void Konsole::EscapeSequenceUrlExtractor::toggleUrlInput()
{
    if (_ignoreNextUrlInput) {
        _ignoreNextUrlInput = false;
        return;
    }

    if (_reading) {
        endUrlInput();
    } else {
        beginUrlInput();
    }
}

}
