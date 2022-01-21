/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "FilterChain.h"
#include "Filter.h"

#include "terminalDisplay/TerminalColor.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"

#include <QDebug>
#include <QEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QRect>

#include <algorithm>

using namespace Konsole;
FilterChain::FilterChain(TerminalDisplay *terminalDisplay)
    : _terminalDisplay(terminalDisplay)
    , _showUrlHint(false)
    , _reverseUrlHints(false)
    , _urlHintsModifiers(Qt::NoModifier)
{
}
FilterChain::~FilterChain()
{
    qDeleteAll(_filters);
}

void FilterChain::addFilter(Filter *filter)
{
    _filters.append(filter);
}

void FilterChain::removeFilter(Filter *filter)
{
    _filters.removeAll(filter);
}

void FilterChain::reset()
{
    for (auto *filter : _filters) {
        filter->reset();
    }
}

void FilterChain::setBuffer(const QString *buffer, const QList<int> *linePositions)
{
    for (auto *filter : _filters) {
        filter->setBuffer(buffer, linePositions);
    }
}

void FilterChain::process()
{
    for (auto *filter : _filters) {
        filter->process();
    }
}

void FilterChain::clear()
{
    _filters.clear();
}

QSharedPointer<HotSpot> FilterChain::hotSpotAt(int line, int column) const
{
    for (auto *filter : _filters) {
        QSharedPointer<HotSpot> spot = filter->hotSpotAt(line, column);
        if (spot != nullptr) {
            return spot;
        }
    }
    return nullptr;
}

QList<QSharedPointer<HotSpot>> FilterChain::hotSpots() const
{
    QList<QSharedPointer<HotSpot>> list;
    for (auto *filter : _filters) {
        list.append(filter->hotSpots());
    }
    return list;
}

QRegion FilterChain::hotSpotRegion() const
{
    QRegion region;
    for (const auto &hotSpot : hotSpots()) {
        QRect r;
        r.setLeft(hotSpot->startColumn());
        r.setTop(hotSpot->startLine());
        if (hotSpot->startLine() == hotSpot->endLine()) {
            r.setRight(hotSpot->endColumn());
            r.setBottom(hotSpot->endLine());
            region |= _terminalDisplay->imageToWidget(r);
        } else {
            r.setRight(_terminalDisplay->columns());
            r.setBottom(hotSpot->startLine());
            region |= _terminalDisplay->imageToWidget(r);

            r.setLeft(0);

            for (int line = hotSpot->startLine() + 1; line < hotSpot->endLine(); line++) {
                r.moveTop(line);
                region |= _terminalDisplay->imageToWidget(r);
            }

            r.moveTop(hotSpot->endLine());
            r.setRight(hotSpot->endColumn());
            region |= _terminalDisplay->imageToWidget(r);
        }
    }
    return region;
}

int FilterChain::count(HotSpot::Type type) const
{
    const auto hSpots = hotSpots();
    return std::count_if(std::begin(hSpots), std::end(hSpots), [type](const QSharedPointer<HotSpot> &s) {
        return s->type() == type;
    });
}

QList<QSharedPointer<HotSpot>> FilterChain::filterBy(HotSpot::Type type) const
{
    QList<QSharedPointer<HotSpot>> hotspots;
    for (const auto &spot : hotSpots()) {
        if (spot->type() == type) {
            hotspots.append(spot);
        }
    }
    return hotspots;
}

void FilterChain::leaveEvent(TerminalDisplay *td, QEvent *ev)
{
    Q_UNUSED(td)
    Q_UNUSED(ev)
    _showUrlHint = false;
}

void FilterChain::keyReleaseEvent(TerminalDisplay *td, QKeyEvent *ev, int charLine, int charColumn)
{
    if (_showUrlHint) {
        _showUrlHint = false;
        td->update();
    }

    auto spot = hotSpotAt(charLine, charColumn);
    if (spot != nullptr) {
        spot->keyReleaseEvent(td, ev);
    }
}

bool FilterChain::keyPressEvent(TerminalDisplay *td, QKeyEvent *ev, int charLine, int charColumn)
{
    if ((_urlHintsModifiers != 0u) && ev->modifiers() == _urlHintsModifiers) {
        QList<QSharedPointer<HotSpot>> hotspots = filterBy(HotSpot::Link);
        int nHotSpots = hotspots.count();
        int hintSelected = ev->key() - '1';

        // Triggered a Hotspot via shortcut.
        if (hintSelected >= 0 && hintSelected <= 9 && hintSelected < nHotSpots) {
            if (_reverseUrlHints) {
                hintSelected = nHotSpots - hintSelected - 1;
            }
            hotspots.at(hintSelected)->activate();
            _showUrlHint = false;
            td->update();
            return true;
        }

        if (!_showUrlHint) {
            td->processFilters();
            _showUrlHint = true;
            td->update();
        }
    }

    auto spot = hotSpotAt(charLine, charColumn);
    if (spot != nullptr) {
        spot->keyPressEvent(td, ev);
    }
    return false;
}

void FilterChain::mouseMoveEvent(TerminalDisplay *td, QMouseEvent *ev, int charLine, int charColumn)
{
    auto spot = hotSpotAt(charLine, charColumn);
    if (_hotSpotUnderMouse != spot) {
        if (_hotSpotUnderMouse != nullptr) {
            _hotSpotUnderMouse->mouseLeaveEvent(td, ev);
        }
        _hotSpotUnderMouse = spot;
        if (_hotSpotUnderMouse != nullptr) {
            _hotSpotUnderMouse->mouseEnterEvent(td, ev);
        }
    }

    if (spot != nullptr) {
        spot->mouseMoveEvent(td, ev);
    }
}

void FilterChain::mouseReleaseEvent(TerminalDisplay *td, QMouseEvent *ev, int charLine, int charColumn)
{
    auto spot = hotSpotAt(charLine, charColumn);
    if (!spot) {
        return;
    }
    spot->mouseReleaseEvent(td, ev);
}

void FilterChain::paint(TerminalDisplay *td, QPainter &painter)
{
    // get color of character under mouse and use it to draw
    // lines for filters
    QPoint cursorPos = td->mapFromGlobal(QCursor::pos());

    auto [cursorLine, cursorColumn] = td->getCharacterPosition(cursorPos, false);

    Character cursorCharacter = td->getCursorCharacter(std::min(cursorColumn, td->columns() - 1), cursorLine);
    painter.setPen(QPen(cursorCharacter.foregroundColor.color(td->terminalColor()->colorTable())));

    // iterate over hotspots identified by the display's currently active filters
    // and draw appropriate visuals to indicate the presence of the hotspot

    const auto spots = hotSpots();
    int urlNumber;
    int urlNumInc;

    // TODO: Remove _reverseUrllHints from TerminalDisplay.
    // TODO: Access reverseUrlHints from the profile, here.
    if (_reverseUrlHints) {
        // The URL hint numbering should be 'physically' increasing on the
        // keyboard, so they start at one and end up on 10 (on 0, or at least
        // they used to, but it seems to have changed, I'll fix that later).
        urlNumber = count(HotSpot::Link);
        urlNumInc = -1;
    } else {
        urlNumber = 1;
        urlNumInc = 1;
    }

    for (const auto &spot : spots) {
        QRegion region;
        if (spot->type() == HotSpot::Link || spot->type() == HotSpot::EMailAddress || spot->type() == HotSpot::EscapedUrl || spot->type() == HotSpot::File) {
            QPair<QRegion, QRect> spotRegion =
                spot->region(td->terminalFont()->fontWidth(), td->terminalFont()->fontHeight(), td->columns(), td->contentRect());
            region = spotRegion.first;
            QRect r = spotRegion.second;

            // TODO: Move this paint code to HotSpot->drawHint();
            // TODO: Fix the Url Hints access from the Profile.
            if (_showUrlHint && (spot->type() == HotSpot::Link || spot->type() == HotSpot::File)) {
                if (urlNumber >= 0 && urlNumber < 10) {
                    // Position at the beginning of the URL
                    QRect hintRect(*region.begin());
                    hintRect.setWidth(r.height());
                    painter.fillRect(hintRect, QColor(0, 0, 0, 128));
                    painter.setPen(Qt::white);
                    painter.drawRect(hintRect.adjusted(0, 0, -1, -1));
                    painter.drawText(hintRect, Qt::AlignCenter, QString::number(urlNumber));
                }
                urlNumber += urlNumInc;
            }
        }

        if (spot->startLine() < 0 || spot->endLine() < 0) {
            qDebug() << "ERROR, invalid hotspot:";
            spot->debug();
        }

        for (int line = spot->startLine(); line <= spot->endLine(); line++) {
            int startColumn = 0;
            int endColumn = td->columns() - 1; // TODO use number of _columns which are actually
            // occupied on this line rather than the width of the
            // display in _columns

            // FIXME: the left side condition is always false due to the
            //        endColumn assignment above
            // Check image size so _image[] is valid (see makeImage)
            if (endColumn >= td->columns() || line >= td->lines()) {
                break;
            }

            // ignore whitespace at the end of the lines
            while (td->getCursorCharacter(endColumn, line).isSpace() && endColumn > 0) {
                endColumn--;
            }

            // increment here because the column which we want to set 'endColumn' to
            // is the first whitespace character at the end of the line
            endColumn++;

            if (line == spot->startLine()) {
                startColumn = spot->startColumn();
            }
            if (line == spot->endLine()) {
                endColumn = spot->endColumn();
            }

            // TODO: resolve this comment with the new margin/center code
            // subtract one pixel from
            // the right and bottom so that
            // we do not overdraw adjacent
            // hotspots
            //
            // subtracting one pixel from all sides also prevents an edge case where
            // moving the mouse outside a link could still leave it underlined
            // because the check below for the position of the cursor
            // finds it on the border of the target area
            QRect r;
            r.setCoords(startColumn * td->terminalFont()->fontWidth() + td->contentRect().left(),
                        line * td->terminalFont()->fontHeight() + td->contentRect().top(),
                        endColumn * td->terminalFont()->fontWidth() + td->contentRect().left() - 1,
                        (line + 1) * td->terminalFont()->fontHeight() + td->contentRect().top() - 1);

            // Underline link hotspots
            // TODO: Fix accessing the urlHint here.
            // TODO: Move this code to UrlFilterHotSpot.
            const bool hasMouse = region.contains(td->mapFromGlobal(QCursor::pos()));
            if (((spot->type() == HotSpot::Link || spot->type() == HotSpot::File) && _showUrlHint) || hasMouse) {
                QFontMetrics metrics(td->font());

                // find the baseline (which is the invisible line that the characters in the font sit on,
                // with some having tails dangling below)
                const int baseline = r.bottom() - metrics.descent();
                // find the position of the underline below that
                const int underlinePos = baseline + metrics.underlinePos();
                painter.drawLine(r.left(), underlinePos, r.right(), underlinePos);

                // Marker hotspots simply have a transparent rectangular shape
                // drawn on top of them
            } else if (spot->type() == HotSpot::Marker) {
                // TODO - Do not use a hardcoded color for this
                const bool isCurrentResultLine = (td->screenWindow()->currentResultLine() == (spot->startLine() + td->screenWindow()->currentLine()));
                QColor color = isCurrentResultLine ? QColor(255, 255, 0, 120) : QColor(255, 0, 0, 120);
                painter.fillRect(r, color);
            }
        }
    }
}

void FilterChain::setReverseUrlHints(bool value)
{
    _reverseUrlHints = value;
}

void FilterChain::setUrlHintsModifiers(Qt::KeyboardModifiers value)
{
    _urlHintsModifiers = value;
}
