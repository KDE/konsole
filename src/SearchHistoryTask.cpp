/*
    Copyright 2006-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2009 by Thomas Dreibholz <dreibh@iem.uni-due.de>

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

#include "SearchHistoryTask.h"

#include <QApplication>
#include <QTextStream>

#include "TerminalCharacterDecoder.h"
#include "Emulation.h"

namespace Konsole {

void SearchHistoryTask::addScreenWindow(Session* session , ScreenWindow* searchWindow)
{
    _windows.insert(session, searchWindow);
}

void SearchHistoryTask::execute()
{
    auto iter = QMapIterator<QPointer<Session>,ScreenWindowPtr>(_windows);

    while (iter.hasNext()) {
        iter.next();
        executeOnScreenWindow(iter.key() , iter.value());
    }
}

void SearchHistoryTask::executeOnScreenWindow(const QPointer<Session> &session , const ScreenWindowPtr& window)
{
    Q_ASSERT(session);
    Q_ASSERT(window);

    Emulation* emulation = session->emulation();

    if (!_regExp.pattern().isEmpty()) {
        int pos = -1;
        const bool forwards = (_direction == Enum::ForwardsSearch);
        const int lastLine = window->lineCount() - 1;

        int startLine;
        if (forwards && (_startLine == lastLine)) {
            startLine = 0;
        } else if (!forwards && (_startLine == 0)) {
            startLine = lastLine;
        } else {
            startLine = _startLine + (forwards ? 1 : -1);
        }

        QString string;

        //text stream to read history into string for pattern or regular expression searching
        QTextStream searchStream(&string);

        PlainTextDecoder decoder;
        decoder.setRecordLinePositions(true);

        //setup first and last lines depending on search direction
        int line = startLine;

        //read through and search history in blocks of 10K lines.
        //this balances the need to retrieve lots of data from the history each time
        //(for efficient searching)
        //without using silly amounts of memory if the history is very large.
        const int maxDelta = qMin(window->lineCount(), 10000);
        int delta = forwards ? maxDelta : -maxDelta;

        int endLine = line;
        bool hasWrapped = false;  // set to true when we reach the top/bottom
        // of the output and continue from the other
        // end

        //loop through history in blocks of <delta> lines.
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
                    endLine = qMin(startLine , endLine);
                } else {
                    endLine = qMax(startLine , endLine);
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
            emulation->writeToStream(&decoder, qMin(endLine, line) , qMax(endLine, line));
            decoder.end();

            // line number search below assumes that the buffer ends with a new-line
            string.append(QLatin1Char('\n'));

            if (forwards) {
                pos = string.indexOf(_regExp);
            } else {
                pos = string.lastIndexOf(_regExp);
            }

            //if a match is found, position the cursor on that line and update the screen
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

                emit completed(true);

                return;
            }

            //clear the current block of text and move to the next one
            string.clear();
            line = endLine;
        } while (startLine != endLine);

        // if no match was found, clear selection to indicate this
        window->clearSelection();
        window->notifyOutputChanged();
    }

    emit completed(false);
}
void SearchHistoryTask::highlightResult(const ScreenWindowPtr& window , int findPos)
{
    //work out how many lines into the current block of text the search result was found
    //- looks a little painful, but it only has to be done once per search.

    ////qDebug() << "Found result at line " << findPos;

    //update display to show area of history containing selection
    if ((findPos < window->currentLine()) ||
            (findPos >= (window->currentLine() + window->windowLines()))) {
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

SearchHistoryTask::SearchHistoryTask(QObject* parent)
    : SessionTask(parent)
    , _direction(Enum::BackwardsSearch)
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

}
