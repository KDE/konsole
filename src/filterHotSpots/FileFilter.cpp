/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "FileFilter.h"

#include <QDir>

#include "profile/Profile.h"
#include "session/Session.h"
#include "session/SessionManager.h"

#include "FileFilterHotspot.h"

using namespace Konsole;

// static
QRegularExpression FileFilter::_regex;

FileFilter::FileFilter(Session *session, const QString &wordCharacters)
    : _session(session)
    , _dirPath(QString())
    , _currentDirContents()
{
    _regex = QRegularExpression(concatRegexPattern(wordCharacters), QRegularExpression::DontCaptureOption);
    setRegExp(_regex);
}

QString FileFilter::concatRegexPattern(QString wordCharacters) const
{
    /* The wordCharacters can be a potentially broken regexp,
     * so let's fix it manually if it has some troublesome characters.
     */
    // Add a folder delimiter at the beginning.
    if (wordCharacters.contains(QLatin1Char('/'))) {
        wordCharacters.remove(QLatin1Char('/'));
        wordCharacters.prepend(QStringLiteral("\\/"));
    }

    // Add minus at the end.
    if (wordCharacters.contains(QLatin1Char('-'))) {
        wordCharacters.remove(QLatin1Char('-'));
        wordCharacters.append(QLatin1Char('-'));
    }

    const QString pattern =
        /* First part of the regexp means 'strings with spaces and starting with single quotes'
         * Second part means "Strings with double quotes"
         * Last part means "Everything else plus some special chars
         * This is much smaller, and faster, than the previous regexp
         * on the HotSpot creation we verify if this is indeed a file, so there's
         * no problem on testing on random words on the screen.
         */
        QStringLiteral(R"RX('[^'\n]+')RX") // Matches everything between single quotes.
        + QStringLiteral(R"RX(|"[^\n"]+")RX") // Matches everything inside double quotes
        // Matches a contiguous line of alphanumeric characters plus some special ones
        // defined in the profile. With a special case for strings starting with '/' which
        // denotes a path on Linux.
        // Takes into account line numbers:
        // - grep output with line numbers: "/path/to/file:123"
        // - compiler error output: ":/path/to/file:123:123"
        //
        // ([^\n/\[]/) to not match "https://", and urls starting with "[" are matched by the
        // next | branch (ctest stuff)
        + QStringLiteral(R"RX(|([^\n\s/\[]/)?[\p{L}\w%1]+(:\d+)?(:\d+:)?)RX").arg(wordCharacters)
        // - ctest error output: "[/path/to/file(123)]"
        + QStringLiteral(R"RX(|\[[/\w%1]+\(\d+\)\])RX").arg(wordCharacters);

    return pattern;
}

/**
 * File Filter - Construct a filter that works on local file paths using the
 * posix portable filename character set combined with KDE's mimetype filename
 * extension blob patterns.
 * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_267
 */

QSharedPointer<HotSpot> FileFilter::newHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts)
{
    if (_session.isNull()) {
        return nullptr;
    }

    const QString &filenameRef = capturedTexts.first();
    QStringView filename = filenameRef;
    if (filename.startsWith(QLatin1Char('\'')) && filename.endsWith(QLatin1Char('\''))) {
        filename = filename.mid(1, filename.size() - 2);
    }

    // '.' and '..' could be valid hotspots, but '..................' most likely isn't
    static const QRegularExpression allDotRe{QRegularExpression::anchoredPattern(QStringLiteral("\\.{3}"))};
    if (allDotRe.match(filename).hasMatch()) {
        return nullptr;
    }

    if (filename.startsWith(QLatin1String("[/"))) { // ctest error output
        filename = filename.mid(1);
    }

    const bool absolute = filename.startsWith(QLatin1Char('/'));
    if (!absolute) {
        auto match = std::find_if(_currentDirContents.cbegin(), _currentDirContents.cend(), [filename](const QString &s) {
            // early out if first char doesn't match
            if (!s.isEmpty() && filename.at(0) != s.at(0)) {
                return false;
            }

            const bool startsWith = filename.startsWith(s);
            if (startsWith) {
                // are we equal ?
                if (filename.size() == s.size()) {
                    return true;
                }
                int onePast = s.size();
                if (onePast < filename.size()) {
                    if (filename.at(onePast) == QLatin1Char(':') || filename.at(onePast) == QLatin1Char('/')) {
                        return true;
                    }
                }
            }
            return false;
        });

        if (match == _currentDirContents.cend()) {
            return nullptr;
        }
    }

    return QSharedPointer<HotSpot>(new FileFilterHotSpot(startLine,
                                                         startColumn,
                                                         endLine,
                                                         endColumn,
                                                         capturedTexts,
                                                         !absolute ? _dirPath + filename.toString() : filename.toString(),
                                                         _session));
}

void FileFilter::process()
{
    const QDir dir(_session->currentWorkingDirectory());
    // Do not re-process.
    if (_dirPath != dir.canonicalPath() + QLatin1Char('/')) {
        _dirPath = dir.canonicalPath() + QLatin1Char('/');

        _currentDirContents = dir.entryList(QDir::Dirs | QDir::Files);
    }

    RegExpFilter::process();
}

void FileFilter::updateRegex(const QString &wordCharacters)
{
    _regex.setPattern(concatRegexPattern(wordCharacters));
    setRegExp(_regex);
}
