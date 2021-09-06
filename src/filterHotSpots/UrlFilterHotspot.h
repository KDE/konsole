/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef URL_FILTER_HOTSPOT
#define URL_FILTER_HOTSPOT

#include "RegExpFilterHotspot.h"

#include <QList>
class QAction;

namespace Konsole
{
/**
 * Hotspot type created by UrlFilter instances.  The activate() method opens a web browser
 * at the given URL when called.
 */
class UrlFilterHotSpot : public RegExpFilterHotSpot
{
public:
    UrlFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts);
    ~UrlFilterHotSpot() override;

    QList<QAction *> actions() override;

    /**
     * Open a web browser at the current URL.  The url itself can be determined using
     * the capturedTexts() method.
     */
    void activate(QObject *object = nullptr) override;

private:
    enum UrlType {
        StandardUrl,
        Email,
        Unknown,
    };
    UrlType urlType() const;
};

}
#endif
