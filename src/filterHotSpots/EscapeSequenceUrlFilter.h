/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef ESCAPE_SEQUENCE_URL_FILTER
#define ESCAPE_SEQUENCE_URL_FILTER

#include "Filter.h"

#include <QPointer>

namespace Konsole
{
class Session;
class TerminalDisplay;

/* This filter is different from the Url filter as there's no
 * URL's in the screen. Vt102Emulation will store a vector of
 * URL/Text, we need to match if this is in the screen. For that we need a pointer
 * for the Vt102Emulation or at least the data structure that holds the information
 * so we can create the hotspots.
 */
class EscapeSequenceUrlFilter : public Filter
{
public:
    EscapeSequenceUrlFilter(Session *session, TerminalDisplay *display);

    void process() override;

private:
    QPointer<Session> _session;
    QPointer<TerminalDisplay> _window;
};
}
#endif
