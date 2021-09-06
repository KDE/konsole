/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "HotSpot.h"

#include <QDebug>
#include <QMouseEvent>

#include "FileFilterHotspot.h"
#include "terminalDisplay/TerminalDisplay.h"
#include "terminalDisplay/TerminalFonts.h"

using namespace Konsole;

HotSpot::~HotSpot() = default;

HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn)
    : _startLine(startLine)
    , _startColumn(startColumn)
    , _endLine(endLine)
    , _endColumn(endColumn)
    , _type(NotSpecified)
{
}

QList<QAction *> HotSpot::actions()
{
    return {};
}

QList<QAction *> HotSpot::setupMenu(QMenu *)
{
    return {};
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
        r.setCoords(startColumn() * fontWidth + left, startLine() * fontHeight + top, (columns)*fontWidth + left - 1, (startLine() + 1) * fontHeight + top - 1);
        region |= r;
        for (int line = startLine() + 1; line < endLine(); line++) {
            r.setCoords(0 * fontWidth + left, line * fontHeight + top, (columns)*fontWidth + left - 1, (line + 1) * fontHeight + top - 1);
            region |= r;
        }
        r.setCoords(0 * fontWidth + left, endLine() * fontHeight + top, (endColumn()) * fontWidth + left - 1, (endLine() + 1) * fontHeight + top - 1);
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

    if (td->cursor().shape() != Qt::PointingHandCursor && ((td->openLinksByDirectClick() || ((ev->modifiers() & Qt::ControlModifier) != 0u)))) {
        td->setCursor(Qt::PointingHandCursor);
    }

    auto r = region(td->terminalFont()->fontWidth(), td->terminalFont()->fontHeight(), td->columns(), td->contentRect()).first;
    td->update(r);
}

void HotSpot::mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    Q_UNUSED(ev)

    if (!isUrl()) {
        return;
    }

    auto r = region(td->terminalFont()->fontWidth(), td->terminalFont()->fontHeight(), td->columns(), td->contentRect()).first;
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

void HotSpot::keyPressEvent(TerminalDisplay *td, QKeyEvent *ev)
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

void HotSpot::debug()
{
    qDebug() << this;
    qDebug() << type();
    qDebug() << _startLine << _endLine << _startColumn << _endColumn;
}

bool Konsole::HotSpot::hasDragOperation() const
{
    return false;
}

void Konsole::HotSpot::startDrag()
{
}
