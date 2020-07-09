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

#ifndef ESCAPE_SEQUENCE_URL_FILTER
#define ESCAPE_SEQUENCE_URL_FILTER

#include "Filter.h"

#include <QPointer>

namespace Konsole {
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
