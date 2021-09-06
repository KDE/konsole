/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "EscapeSequenceUrlExtractor.h"

#include <QUrl>

namespace Konsole
{
EscapeSequenceUrlExtractor::EscapeSequenceUrlExtractor() = default;

void Konsole::EscapeSequenceUrlExtractor::setScreen(Konsole::Screen *screen)
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

void EscapeSequenceUrlExtractor::setUrl(const QString &url)
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

void EscapeSequenceUrlExtractor::setAllowedLinkSchema(const QStringList &schema)
{
    _allowedUriSchemas = schema;
}

void EscapeSequenceUrlExtractor::historyLinesRemoved(int lines)
{
    for (auto &url : _history) {
        url.begin.row -= lines;
        url.end.row -= lines;
    }
    _history.erase(std::remove_if(std::begin(_history),
                                  std::end(_history),
                                  [](const ExtractedUrl &url) {
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
