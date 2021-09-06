/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef REGEXP_FILTER_H
#define REGEXP_FILTER_H

#include "Filter.h"

#include <QRegularExpression>
#include <QSharedPointer>

namespace Konsole
{
class HotSpot;
/**
 * A filter which searches for sections of text matching a regular expression and creates a new RegExpFilter::HotSpot
 * instance for them.
 *
 * Subclasses can reimplement newHotSpot() to return custom hotspot types when matches for the regular expression
 * are found.
 */
class RegExpFilter : public Filter
{
public:
    /** Constructs a new regular expression filter */
    RegExpFilter();

    /**
     * Sets the regular expression which the filter searches for in blocks of text.
     *
     * Regular expressions which match the empty string are treated as not matching
     * anything.
     */
    void setRegExp(const QRegularExpression &regExp);
    /** Returns the regular expression which the filter searches for in blocks of text */
    QRegularExpression regExp() const;

    /**
     * Reimplemented to search the filter's text buffer for text matching regExp()
     *
     * If regexp matches the empty string, then process() will return immediately
     * without finding results.
     */
    void process() override;

protected:
    /**
     * Called when a match for the regular expression is encountered.  Subclasses should reimplement this
     * to return custom hotspot types
     */
    virtual QSharedPointer<HotSpot> newHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts);

private:
    QRegularExpression _searchText;
};

}
#endif
