/*
    SPDX-FileCopyrightText: 2006-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2009 Thomas Dreibholz <dreibh@iem.uni-due.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SearchHistoryTask.h"

#include <QApplication>
#include <QTextStream>

#include "../decoders/PlainTextDecoder.h"
#include "Emulation.h"

namespace Konsole
{
void SearchHistoryTask::addScreenWindow(Session *session, ScreenWindow *searchWindow)
{
    _windows.insert(session, searchWindow);
}

bool SearchHistoryTask::execute()
{
    auto iter = QMapIterator<QPointer<Session>, ScreenWindowPtr>(_windows);

    while (iter.hasNext()) {
        iter.next();
        executeOnScreenWindow(iter.key(), iter.value());
    }

    if (autoDelete()) {
        deleteLater();
    }
    return true;
}

void SearchHistoryTask::executeOnScreenWindow(const QPointer<Session> &session, const ScreenWindowPtr &window)
{
    Q_ASSERT(session);
    Q_ASSERT(window);

    Emulation *emulation = session->emulation();

    if (!_regExp.pattern().isEmpty()) {
        bool forwards = (_direction == Enum::ForwardsSearch);
        const int lastLine = window->lineCount() - 1;

        int startLine = _startLine;
        if (forwards && (_startLine == lastLine)) {
            if (!_noWrap) {
                startLine = 0;
            }
        } else if (!forwards && (_startLine == 0)) {
            if (!_noWrap) {
                startLine = lastLine;
            }
        } else {
            startLine = _startLine + (forwards ? 1 : -1);
        }

        QString string;

        // text stream to read history into string for pattern or regular expression searching
        QTextStream searchStream(&string);

        PlainTextDecoder decoder;
        decoder.setRecordLinePositions(true);

        // setup first and last lines depending on search direction
        int line = startLine;

        // read through and search history in blocks of 10K lines.
        // this balances the need to retrieve lots of data from the history each time
        //(for efficient searching)
        // without using silly amounts of memory if the history is very large.
        const int maxDelta = qMin(window->lineCount(), 10000);
        int delta = forwards ? maxDelta : -maxDelta;

        int endLine = line;
        bool continueLoop = true;
        bool invertDirection = false;
        bool hasWrapped = false; // set to true when we reach the top/bottom
        // of the output and continue from the other
        // end

        QSet<int> matchPositions;

        decoder.begin(&searchStream);
        emulation->writeToStream(&decoder, 0, lastLine);
        decoder.end();
        const QList<int> linePositions = decoder.linePositions();

        QRegularExpressionMatchIterator matchIterator = _regExp.globalMatch(string);
        while (matchIterator.hasNext()) {
            const QRegularExpressionMatch match = matchIterator.next();
            const qsizetype startPos = match.capturedStart();
            if (startPos != -1) {
                const auto lineMatch = std::upper_bound(linePositions.begin(), linePositions.end(), startPos);
                if (lineMatch != linePositions.end()) {
                    const auto lineIdx = qMin(0ll, lastLine) + std::distance(linePositions.begin(), lineMatch) - 1;
                    matchPositions.insert(static_cast<int>(lineIdx));
                }
            }
        }

        Q_EMIT searchResults(matchPositions, window->lineCount());

        string.clear();

        // loop through history in blocks of <delta> lines.
        do {
            // ensure that application does not appear to hang
            // if searching through a lengthy output
            QApplication::processEvents();

            // calculate lines to search in this iteration
            if (hasWrapped) {
                if (endLine == lastLine) {
                    line = 0;
                } else if (endLine == 0) {
                    line = lastLine;
                }

                endLine += delta;

                if (forwards) {
                    if (endLine >= startLine) {
                        endLine = startLine;
                        continueLoop = false;
                    }
                } else {
                    if (endLine <= startLine) {
                        endLine = startLine;
                        continueLoop = false;
                    }
                }
            } else {
                endLine += delta;

                if (endLine > lastLine) {
                    hasWrapped = true;
                    endLine = lastLine;
                } else if (endLine < 0) {
                    hasWrapped = true;
                    endLine = 0;
                }
            }

            decoder.begin(&searchStream);
            emulation->writeToStream(&decoder, qMin(endLine, line), qMax(endLine, line));
            decoder.end();

            // line number search below assumes that the buffer ends with a new-line
            string.append(QLatin1Char('\n'));

            const auto pos = forwards ? string.indexOf(_regExp) : string.lastIndexOf(_regExp);

            // if a match is found, position the cursor on that line and update the screen
            if (pos != -1) {
                int newLines = 0;
                QList<int> linePositions = decoder.linePositions();
                while (newLines < linePositions.count() && linePositions[newLines] <= pos) {
                    newLines++;
                }

                // ignore the new line at the start of the buffer
                newLines--;

                int findPos = qMin(line, endLine) + newLines;

                highlightResult(window, findPos);

                Q_EMIT completed(true);

                return;
            }

            // if noWrap is checked and we wrapped, set cursor at last match
            if (hasWrapped && _noWrap) {
                // one chance to invert search direction
                if (invertDirection) {
                    continueLoop = false;
                }
                invertDirection = true;
                forwards = !forwards;
                delta = -delta;
                endLine += (forwards ? 1 : -1);
                hasWrapped = false;
            }

            // clear the current block of text and move to the next one
            string.clear();
            line = endLine;
        } while (continueLoop);
        if (!session->getSelectMode()) {
            // if no match was found, clear selection to indicate this,
            window->clearSelection();
            window->notifyOutputChanged();

            Q_EMIT searchResults(QSet<int>{}, window->lineCount());
        }
    }

    Q_EMIT completed(false);
}
void SearchHistoryTask::highlightResult(const ScreenWindowPtr &window, int findPos)
{
    // work out how many lines into the current block of text the search result was found
    //- looks a little painful, but it only has to be done once per search.

    ////qDebug() << "Found result at line " << findPos;

    // update display to show area of history containing selection
    if ((findPos < window->currentLine()) || (findPos >= (window->currentLine() + window->windowLines()))) {
        int centeredScrollPos = findPos - window->windowLines() / 2;
        if (centeredScrollPos < 0) {
            centeredScrollPos = 0;
        }

        window->scrollTo(centeredScrollPos);
    }

    window->setTrackOutput(false);
    window->notifyOutputChanged();
    window->setCurrentResultLine(findPos);
}

SearchHistoryTask::SearchHistoryTask(QObject *parent)
    : SessionTask(parent)
    , _direction(Enum::BackwardsSearch)
    , _noWrap(false)
    , _startLine(0)
{
}

void SearchHistoryTask::setSearchDirection(Enum::SearchDirection direction)
{
    _direction = direction;
}

void SearchHistoryTask::setStartLine(int line)
{
    _startLine = line;
}

Enum::SearchDirection SearchHistoryTask::searchDirection() const
{
    return _direction;
}

void SearchHistoryTask::setRegExp(const QRegularExpression &expression)
{
    _regExp = expression;
}

QRegularExpression SearchHistoryTask::regExp() const
{
    return _regExp;
}

void SearchHistoryTask::setNoWrap(bool noWrap)
{
    _noWrap = noWrap;
}

bool SearchHistoryTask::noWrap() const
{
    return _noWrap;
}
}

#include "moc_SearchHistoryTask.cpp"
