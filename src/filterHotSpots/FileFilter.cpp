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

#include "FileFilter.h"

#include <QDir>

#include "session/Session.h"
#include "session/SessionManager.h"
#include "profile/Profile.h"

#include "FileFilterHotspot.h"

using namespace Konsole;

FileFilter::FileFilter(Session *session) :
    _session(session)
    , _dirPath(QString())
    , _currentDirContents()
{
    Profile::Ptr profile = SessionManager::instance()->sessionProfile(_session);
    QString wordCharacters = profile->wordCharacters();

    /* The wordCharacters can be a potentially broken regexp,
     * so let's fix it manually if it has some troublesome characters.
     */
    // Add a folder delimiter at the beginning.
    if (wordCharacters.contains(QLatin1Char('/'))) {
        wordCharacters.remove(QLatin1Char('/'));
        wordCharacters.prepend(QStringLiteral("\\/"));
    }

    // Add minus at the end.
    if (wordCharacters.contains(QLatin1Char('-'))){
        wordCharacters.remove(QLatin1Char('-'));
        wordCharacters.append(QLatin1Char('-'));
    }

    static auto re = QRegularExpression(
        /* First part of the regexp means 'strings with spaces and starting with single quotes'
         * Second part means "Strings with double quotes"
         * Last part means "Everything else plus some special chars
         * This is much smaller, and faster, than the previous regexp
         * on the HotSpot creation we verify if this is indeed a file, so there's
         * no problem on testing on random words on the screen.
         */
            QLatin1String("'[^']+'")             // Matches everything between single quotes.
            + QStringLiteral(R"RX(|"[^"]+")RX")      // Matches everything inside double quotes
            + QStringLiteral(R"RX(|[\p{L}\w%1]+)RX").arg(wordCharacters) // matches a contiguous line of alphanumeric characters plus some special ones defined in the profile.
        ,
        QRegularExpression::DontCaptureOption);
    setRegExp(re);
}


/**
  * File Filter - Construct a filter that works on local file paths using the
  * posix portable filename character set combined with KDE's mimetype filename
  * extension blob patterns.
  * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_267
  */

QSharedPointer<HotSpot> FileFilter::newHotSpot(int startLine, int startColumn, int endLine,
                                              int endColumn, const QStringList &capturedTexts)
{
    if (_session.isNull()) {
        return nullptr;
    }

    QString filename = capturedTexts.first();
    if (filename.startsWith(QLatin1Char('\'')) && filename.endsWith(QLatin1Char('\''))) {
        filename.remove(0, 1);
        filename.chop(1);
    }

    // Return nullptr if it's not:
    // <current dir>/filename
    // <current dir>/childDir/filename
    auto match = std::find_if(_currentDirContents.cbegin(), _currentDirContents.cend(),
                              [filename](const QString &s) { return filename.startsWith(s); });

    if (match == _currentDirContents.cend()) {
        return nullptr;
    }

    return QSharedPointer<HotSpot>(new FileFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts, _dirPath + filename));
}

void FileFilter::process()
{
    const QDir dir(_session->currentWorkingDirectory());
    // Do not re-process.
    if (_dirPath != dir.canonicalPath() + QLatin1Char('/')) {
        _dirPath = dir.canonicalPath() + QLatin1Char('/');
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)

        const auto tmpList = dir.entryList(QDir::Dirs | QDir::Files);
        _currentDirContents = QSet<QString>(std::begin(tmpList), std::end(tmpList));

#else
        _currentDirContents = QSet<QString>::fromList(dir.entryList(QDir::Dirs | QDir::Files));
#endif
    }

    RegExpFilter::process();
}
