/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FILTER_H
#define FILTER_H

// Qt
#include <QList>
#include <QMultiHash>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QRegularExpression>
#include <QSet>
#include <QStringList>

// KDE
#include <KFileItem>
#include <KFileItemActions>
#include <KIO/PreviewJob>

#include <memory>

// Konsole
#include "../characters/Character.h"

class QAction;
class QMenu;
class QMouseEvent;

namespace Konsole
{
class Session;
class HotSpot;

/**
 * A filter processes blocks of text looking for certain patterns (such as URLs or keywords from a list)
 * and marks the areas which match the filter's patterns as 'hotspots'.
 *
 * Each hotspot has a type identifier associated with it ( such as a link or a highlighted section ),
 * and an action.  When the user performs some activity such as a mouse-click in a hotspot area ( the exact
 * action will depend on what is displaying the block of text which the filter is processing ), the hotspot's
 * activate() method should be called.  Depending on the type of hotspot this will trigger a suitable response.
 *
 * For example, if a hotspot represents a URL then a suitable action would be opening that URL in a web browser.
 * Hotspots may have more than one action, in which case the list of actions can be obtained using the
 * actions() method.
 *
 * Different subclasses of filter will return different types of hotspot.
 * Subclasses must reimplement the process() method to examine a block of text and identify sections of interest.
 * When processing the text they should create instances of Filter::HotSpot subclasses for sections of interest
 * and add them to the filter's list of hotspots using addHotSpot()
 */
class Filter
{
public:
    /** Constructs a new filter. */
    Filter();
    virtual ~Filter();

    /** Causes the filter to process the block of text currently in its internal buffer */
    virtual void process() = 0;

    /**
     * Empties the filters internal buffer and resets the line count back to 0.
     * All hotspots are deleted.
     */
    void reset();

    /** Returns the hotspot which covers the given @p line and @p column, or 0 if no hotspot covers that area */
    QSharedPointer<HotSpot> hotSpotAt(int line, int column) const;

    /** Returns the list of hotspots identified by the filter */
    QList<QSharedPointer<HotSpot>> hotSpots() const;

    /** Returns the list of hotspots identified by the filter which occur on a given line */

    /**
     * TODO: Document me
     */
    void setBuffer(const QString *buffer, const QList<int> *linePositions);

protected:
    /** Adds a new hotspot to the list */
    void addHotSpot(QSharedPointer<HotSpot> spot);
    /** Returns the internal buffer */
    const QString *buffer();
    /** Converts a character position within buffer() to a line and column */
    std::pair<int, int> getLineColumn(int prevline, int position);

private:
    Q_DISABLE_COPY(Filter)

    QMultiHash<int, QSharedPointer<HotSpot>> _hotspots;
    QList<QSharedPointer<HotSpot>> _hotspotList;

    const QList<int> *_linePositions;
    const QString *_buffer;
};

} // namespace Konsole
#endif
