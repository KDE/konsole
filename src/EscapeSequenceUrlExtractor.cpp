/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "EscapeSequenceUrlExtractor.h"
#include "Screen.h"

#include <QHostInfo>
#include <QUrl>

namespace Konsole
{
EscapeSequenceUrlExtractor::EscapeSequenceUrlExtractor() = default;

void Konsole::EscapeSequenceUrlExtractor::setScreen(Konsole::Screen *screen)
{
    _screen = screen;
    clear();
}

void EscapeSequenceUrlExtractor::beginUrlInput()
{
    _reading = true;
}

void EscapeSequenceUrlExtractor::appendUrlText_impl(uint c)
{
    if (_currentUrl.text.isEmpty()) {
        // We need to  on getCursorX because we want the position of the
        // last printed character, not the cursor.
        const int realCcolumn = _screen->getCursorY() + _screen->getHistLines();
        _currentUrl.begin = Coordinate{realCcolumn, _screen->getCursorX() - 1};
    }
    _currentUrl.text += QString::fromUcs4(&c, 1);
}

void EscapeSequenceUrlExtractor::setUrl(const QString &url)
{
    QUrl qUrl = QUrl(url);

    if (_allowedUriSchemas.contains(qUrl.scheme() + QLatin1String("://"))) {
        if (qUrl.scheme() == QLatin1String("file") && !qUrl.host().isEmpty()) {
            if (qUrl.host() != QHostInfo::localHostName() && qUrl.host() != QLatin1String("localhost")) {
                abortUrlInput();
                return;
            }

            qUrl.setHost(QString());
        }

        _currentUrl.url = qUrl.toString();
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

void EscapeSequenceUrlExtractor::clearBetween(int loca, int loce)
{
    _history.removeIf([&](const ExtractedUrl &url) {
        int beginLoc = url.begin.row * _screen->getColumns() + url.begin.col;
        int endLoc = url.end.row * _screen->getColumns() + url.end.col;

        return (loca <= beginLoc && beginLoc <= loce) || (loca <= endLoc && endLoc <= loce);
    });
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

#include "moc_EscapeSequenceUrlExtractor.cpp"
