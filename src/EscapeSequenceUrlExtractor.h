#ifndef ESCAPE_SEQUENCE_URL_EXTRACTOR_H
#define ESCAPE_SEQUENCE_URL_EXTRACTOR_H

/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 1997, 1998 Lars Doelle <lars.doelle@on-line.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "Screen.h"
#include <QObject>
#include <QPointer>

namespace Konsole
{
/* Like QPoint, but with Row / Col
 * easier to read than x / y
 */
struct Coordinate {
    int row;
    int col;
};

/* Represents a URL on the visual part of konsole, that has been escaped.
 * like a html url tag with a text value.
 */
struct ExtractedUrl {
    QString url;
    QString text;
    Coordinate begin;
    Coordinate end;
};

/* Stored in Screen, but used in V10Emulation to
 * store extracted URL's. Perhaps this should be a Model?
 */
class EscapeSequenceUrlExtractor
{
private:
    /* Tell us if we are currently reading or not a URL. */
    bool _reading = false;

    /* If we abort reading a URL input we enter in a invalid state,
     * and we need to ignore the next toggle.
     */
    bool _ignoreNextUrlInput = false;

    /* The url / text pair being extracted currently */
    ExtractedUrl _currentUrl;

    /* The maximum size of url to prevent a bomb
     * that will take over the history file.
     * TODO: make it configurable.
     */
    // Not used ATM const int _maximumUrlHistory = 200;

    /* All of the extracted URL's. */
    QVector<ExtractedUrl> _history;

    /* The URI schema format that's accepted */
    QStringList _allowedUriSchemas;

    /* Pointer to the Screen, that actually holds the text data. */
    Screen *_screen;

public:
    /* This needs to have access to the Session
     * calculate the row / col of the current URL.
     */
    EscapeSequenceUrlExtractor();

    /* This is a list of URI schemas that are going to be supported, separated by semicolon.
     * like https://;file://
     */
    void setAllowedLinkSchema(const QStringList &allowedSchemas);

    void setScreen(Screen *screen);

    /* If we are parsing a URL */
    bool reading() const;

    /* We found an URL, starting to parse */
    void beginUrlInput();

    /* We received the end byte to finish the Url. */
    void endUrlInput();

    /* We are not saving this URL, it's bogus. */
    void abortUrlInput();

    /* The URL is parsed at once, but not the text. We received
     * one character per time until we hit the end byte. */
    void appendUrlText(QChar c);

    /* The URL is parsed at once, store it at once. */
    void setUrl(const QString &url);

    /* All of the parsedURL's, used by TerminalDisplay to paint them
     * on screen. */
    QVector<ExtractedUrl> history() const;

    /* Clear all the URL's, this is triggered when the Screen is cleared. */
    void clear();

    /* Iterates trough all the URL's and remove the ones that are currently
     * out of bounds because we removed lines in the History
     */
    void historyLinesRemoved(int lines);

    /* starts / stops URL Processing */
    void toggleUrlInput();
};

}

#endif
