/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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

// Own
#include "Filter.h"

#include "konsoledebug.h"

// Qt
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QMimeDatabase>
#include <QString>
#include <QTextStream>
#include <QUrl>

// KDE
#include <KLocalizedString>
#include <KRun>

// Konsole
#include "Session.h"
#include "TerminalCharacterDecoder.h"

using namespace Konsole;

FilterChain::~FilterChain()
{
    QMutableListIterator<Filter *> iter(*this);

    while (iter.hasNext()) {
        Filter *filter = iter.next();
        iter.remove();
        delete filter;
    }
}

void FilterChain::addFilter(Filter *filter)
{
    append(filter);
}

void FilterChain::removeFilter(Filter *filter)
{
    removeAll(filter);
}

void FilterChain::reset()
{
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext()) {
        iter.next()->reset();
    }
}

void FilterChain::setBuffer(const QString *buffer, const QList<int> *linePositions)
{
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext()) {
        iter.next()->setBuffer(buffer, linePositions);
    }
}

void FilterChain::process()
{
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext()) {
        iter.next()->process();
    }
}

void FilterChain::clear()
{
    QList<Filter *>::clear();
}

Filter::HotSpot *FilterChain::hotSpotAt(int line, int column) const
{
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext()) {
        Filter *filter = iter.next();
        Filter::HotSpot *spot = filter->hotSpotAt(line, column);
        if (spot != nullptr) {
            return spot;
        }
    }

    return nullptr;
}

QList<Filter::HotSpot *> FilterChain::hotSpots() const
{
    QList<Filter::HotSpot *> list;
    QListIterator<Filter *> iter(*this);
    while (iter.hasNext()) {
        Filter *filter = iter.next();
        list << filter->hotSpots();
    }
    return list;
}

TerminalImageFilterChain::TerminalImageFilterChain() :
    _buffer(nullptr),
    _linePositions(nullptr)
{
}

TerminalImageFilterChain::~TerminalImageFilterChain()
{
    delete _buffer;
    delete _linePositions;
}

void TerminalImageFilterChain::setImage(const Character * const image, int lines, int columns,
                                        const QVector<LineProperty> &lineProperties)
{
    if (empty()) {
        return;
    }

    // reset all filters and hotspots
    reset();

    PlainTextDecoder decoder;
    decoder.setLeadingWhitespace(true);
    decoder.setTrailingWhitespace(true);

    // setup new shared buffers for the filters to process on
    auto newBuffer = new QString();
    auto newLinePositions = new QList<int>();
    setBuffer(newBuffer, newLinePositions);

    // free the old buffers
    delete _buffer;
    delete _linePositions;

    _buffer = newBuffer;
    _linePositions = newLinePositions;

    QTextStream lineStream(_buffer);
    decoder.begin(&lineStream);

    for (int i = 0; i < lines; i++) {
        _linePositions->append(_buffer->length());
        decoder.decodeLine(image + i * columns, columns, LINE_DEFAULT);

        // pretend that each line ends with a newline character.
        // this prevents a link that occurs at the end of one line
        // being treated as part of a link that occurs at the start of the next line
        //
        // the downside is that links which are spread over more than one line are not
        // highlighted.
        //
        // TODO - Use the "line wrapped" attribute associated with lines in a
        // terminal image to avoid adding this imaginary character for wrapped
        // lines
        if ((lineProperties.value(i, LINE_DEFAULT) & LINE_WRAPPED) == 0) {
            lineStream << QLatin1Char('\n');
        }
    }
    decoder.end();
}

Filter::Filter() :
    _hotspots(QMultiHash<int, HotSpot *>()),
    _hotspotList(QList<HotSpot *>()),
    _linePositions(nullptr),
    _buffer(nullptr)
{
}

Filter::~Filter()
{
    reset();
}

void Filter::reset()
{
    _hotspots.clear();
    QListIterator<HotSpot *> iter(_hotspotList);
    while (iter.hasNext()) {
        delete iter.next();
    }
    _hotspotList.clear();
}

void Filter::setBuffer(const QString *buffer, const QList<int> *linePositions)
{
    _buffer = buffer;
    _linePositions = linePositions;
}

void Filter::getLineColumn(int position, int &startLine, int &startColumn)
{
    Q_ASSERT(_linePositions);
    Q_ASSERT(_buffer);

    for (int i = 0; i < _linePositions->count(); i++) {
        int nextLine = 0;

        if (i == _linePositions->count() - 1) {
            nextLine = _buffer->length() + 1;
        } else {
            nextLine = _linePositions->value(i + 1);
        }

        if (_linePositions->value(i) <= position && position < nextLine) {
            startLine = i;
            startColumn = Character::stringWidth(buffer()->mid(_linePositions->value(i),
                                                     position - _linePositions->value(i)));
            return;
        }
    }
}

const QString *Filter::buffer()
{
    return _buffer;
}

Filter::HotSpot::~HotSpot() = default;

void Filter::addHotSpot(HotSpot *spot)
{
    _hotspotList << spot;

    for (int line = spot->startLine(); line <= spot->endLine(); line++) {
        _hotspots.insert(line, spot);
    }
}

QList<Filter::HotSpot *> Filter::hotSpots() const
{
    return _hotspotList;
}

Filter::HotSpot *Filter::hotSpotAt(int line, int column) const
{
    QList<HotSpot *> hotspots = _hotspots.values(line);

    foreach (HotSpot *spot, hotspots) {
        if (spot->startLine() == line && spot->startColumn() > column) {
            continue;
        }
        if (spot->endLine() == line && spot->endColumn() < column) {
            continue;
        }

        return spot;
    }

    return nullptr;
}

Filter::HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn) :
    _startLine(startLine),
    _startColumn(startColumn),
    _endLine(endLine),
    _endColumn(endColumn),
    _type(NotSpecified)
{
}

QList<QAction *> Filter::HotSpot::actions()
{
    return QList<QAction *>();
}

int Filter::HotSpot::startLine() const
{
    return _startLine;
}

int Filter::HotSpot::endLine() const
{
    return _endLine;
}

int Filter::HotSpot::startColumn() const
{
    return _startColumn;
}

int Filter::HotSpot::endColumn() const
{
    return _endColumn;
}

Filter::HotSpot::Type Filter::HotSpot::type() const
{
    return _type;
}

void Filter::HotSpot::setType(Type type)
{
    _type = type;
}

RegExpFilter::RegExpFilter() :
    _searchText(QRegularExpression())
{
}

RegExpFilter::HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn,
                               const QStringList &capturedTexts) :
    Filter::HotSpot(startLine, startColumn, endLine, endColumn),
    _capturedTexts(capturedTexts)
{
    setType(Marker);
}

void RegExpFilter::HotSpot::activate(QObject *)
{
}

QStringList RegExpFilter::HotSpot::capturedTexts() const
{
    return _capturedTexts;
}

void RegExpFilter::setRegExp(const QRegularExpression &regExp)
{
    _searchText = regExp;
    _searchText.optimize();
}

QRegularExpression RegExpFilter::regExp() const
{
    return _searchText;
}

void RegExpFilter::process()
{
    const QString *text = buffer();

    Q_ASSERT(text);

    if (!_searchText.isValid() || _searchText.pattern().isEmpty()) {
        return;
    }

    QRegularExpressionMatchIterator iterator(_searchText.globalMatch(*text));
    while (iterator.hasNext()) {
        QRegularExpressionMatch match(iterator.next());

        int startLine = 0;
        int endLine = 0;
        int startColumn = 0;
        int endColumn = 0;

        getLineColumn(match.capturedStart(), startLine, startColumn);
        getLineColumn(match.capturedEnd(), endLine, endColumn);

        RegExpFilter::HotSpot *spot = newHotSpot(startLine, startColumn,
                                                 endLine, endColumn, match.capturedTexts());
        if (spot == nullptr) {
            continue;
        }

        addHotSpot(spot);
    }
}

RegExpFilter::HotSpot *RegExpFilter::newHotSpot(int startLine, int startColumn, int endLine,
                                                int endColumn, const QStringList &capturedTexts)
{
    return new RegExpFilter::HotSpot(startLine, startColumn,
                                     endLine, endColumn, capturedTexts);
}

RegExpFilter::HotSpot *UrlFilter::newHotSpot(int startLine, int startColumn, int endLine,
                                             int endColumn, const QStringList &capturedTexts)
{
    return new UrlFilter::HotSpot(startLine, startColumn,
                                  endLine, endColumn, capturedTexts);
}

UrlFilter::HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn,
                            const QStringList &capturedTexts) :
    RegExpFilter::HotSpot(startLine, startColumn, endLine, endColumn, capturedTexts),
    _urlObject(new FilterObject(this))
{
    setType(Link);
}

UrlFilter::HotSpot::UrlType UrlFilter::HotSpot::urlType() const
{
    const QString url = capturedTexts().at(0);

    if (FullUrlRegExp.match(url).hasMatch()) {
        return StandardUrl;
    } else if (EmailAddressRegExp.match(url).hasMatch()) {
        return Email;
    } else {
        return Unknown;
    }
}

void UrlFilter::HotSpot::activate(QObject *object)
{
    QString url = capturedTexts().at(0);

    const UrlType kind = urlType();

    const QString &actionName = object != nullptr ? object->objectName() : QString();

    if (actionName == QLatin1String("copy-action")) {
        QApplication::clipboard()->setText(url);
        return;
    }

    if ((object == nullptr) || actionName == QLatin1String("open-action")) {
        if (kind == StandardUrl) {
            // if the URL path does not include the protocol ( eg. "www.kde.org" ) then
            // prepend http:// ( eg. "www.kde.org" --> "http://www.kde.org" )
            if (!url.contains(QLatin1String("://"))) {
                url.prepend(QLatin1String("http://"));
            }
        } else if (kind == Email) {
            url.prepend(QLatin1String("mailto:"));
        }

        new KRun(QUrl(url), QApplication::activeWindow());
    }
}

// Note:  Altering these regular expressions can have a major effect on the performance of the filters
// used for finding URLs in the text, especially if they are very general and could match very long
// pieces of text.
// Please be careful when altering them.

//regexp matches:
// full url:
// protocolname:// or www. followed by anything other than whitespaces, <, >, ' or ", and ends before whitespaces, <, >, ', ", ], !, ), :, comma and dot
const QRegularExpression UrlFilter::FullUrlRegExp(QStringLiteral("(www\\.(?!\\.)|[a-z][a-z0-9+.-]*://)[^\\s<>'\"]+[^!,\\.\\s<>'\"\\]\\)\\:]"),
                                                  QRegularExpression::OptimizeOnFirstUsageOption);
// email address:
// [word chars, dots or dashes]@[word chars, dots or dashes].[word chars]
const QRegularExpression UrlFilter::EmailAddressRegExp(QStringLiteral("\\b(\\w|\\.|-|\\+)+@(\\w|\\.|-)+\\.\\w+\\b"),
                                                       QRegularExpression::OptimizeOnFirstUsageOption);

// matches full url or email address
const QRegularExpression UrlFilter::CompleteUrlRegExp(QLatin1Char('(') + FullUrlRegExp.pattern() + QLatin1Char('|')
                                                      + EmailAddressRegExp.pattern() + QLatin1Char(')'),
                                                      QRegularExpression::OptimizeOnFirstUsageOption);

UrlFilter::UrlFilter()
{
    setRegExp(CompleteUrlRegExp);
}

UrlFilter::HotSpot::~HotSpot()
{
    delete _urlObject;
}

void FilterObject::activated()
{
    _filter->activate(sender());
}

QList<QAction *> UrlFilter::HotSpot::actions()
{
    auto openAction = new QAction(_urlObject);
    auto copyAction = new QAction(_urlObject);

    const UrlType kind = urlType();
    Q_ASSERT(kind == StandardUrl || kind == Email);

    if (kind == StandardUrl) {
        openAction->setText(i18n("Open Link"));
        copyAction->setText(i18n("Copy Link Address"));
    } else if (kind == Email) {
        openAction->setText(i18n("Send Email To..."));
        copyAction->setText(i18n("Copy Email Address"));
    }

    // object names are set here so that the hotspot performs the
    // correct action when activated() is called with the triggered
    // action passed as a parameter.
    openAction->setObjectName(QStringLiteral("open-action"));
    copyAction->setObjectName(QStringLiteral("copy-action"));

    QObject::connect(openAction, &QAction::triggered, _urlObject,
                     &Konsole::FilterObject::activated);
    QObject::connect(copyAction, &QAction::triggered, _urlObject,
                     &Konsole::FilterObject::activated);

    QList<QAction *> actions;
    actions << openAction;
    actions << copyAction;

    return actions;
}

/**
  * File Filter - Construct a filter that works on local file paths using the
  * posix portable filename character set combined with KDE's mimetype filename
  * extension blob patterns.
  * http://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_267
  */

RegExpFilter::HotSpot *FileFilter::newHotSpot(int startLine, int startColumn, int endLine,
                                              int endColumn, const QStringList &capturedTexts)
{
    if (_session.isNull()) {
        qCDebug(KonsoleDebug) << "Trying to create new hot spot without session!";
        return nullptr;
    }

    QString filename = capturedTexts.first();
    if (filename.startsWith(QLatin1Char('\'')) && filename.endsWith(QLatin1Char('\''))) {
        filename.remove(0, 1);
        filename.chop(1);
    }

    if (!_currentFiles.contains(filename)) {
        return nullptr;
    }

    return new FileFilter::HotSpot(startLine, startColumn, endLine, endColumn, capturedTexts, _dirPath + filename);
}

void FileFilter::process()
{
    const QDir dir(_session->currentWorkingDirectory());
    _dirPath = dir.canonicalPath() + QLatin1Char('/');
    _currentFiles = dir.entryList(QDir::Files).toSet();

    RegExpFilter::process();
}

FileFilter::HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn,
                             const QStringList &capturedTexts, const QString &filePath) :
    RegExpFilter::HotSpot(startLine, startColumn, endLine, endColumn, capturedTexts),
    _fileObject(new FilterObject(this)),
    _filePath(filePath)
{
    setType(Link);
}

void FileFilter::HotSpot::activate(QObject *)
{
    new KRun(QUrl::fromLocalFile(_filePath), QApplication::activeWindow());
}

static QString createFileRegex(const QStringList &patterns, const QString &filePattern, const QString &pathPattern)
{
    QStringList suffixes = patterns.filter(QRegularExpression(QStringLiteral("^\\*") + filePattern + QStringLiteral("$")));
    QStringList prefixes = patterns.filter(QRegularExpression(QStringLiteral("^") + filePattern + QStringLiteral("+\\*$")));
    QStringList fullNames = patterns.filter(QRegularExpression(QStringLiteral("^") + filePattern + QStringLiteral("$")));

    suffixes.replaceInStrings(QStringLiteral("*"), QString());
    suffixes.replaceInStrings(QStringLiteral("."), QStringLiteral("\\."));
    prefixes.replaceInStrings(QStringLiteral("*"), QString());
    prefixes.replaceInStrings(QStringLiteral("."), QStringLiteral("\\."));

    return QString(
                // Optional path in front
                pathPattern + QLatin1Char('?') +
                // Files with known suffixes
                QLatin1Char('(') +
                  filePattern +
                    QLatin1String("(") +
                      suffixes.join(QLatin1Char('|')) +
                    QLatin1String(")") +
                // Files with known prefixes
                QLatin1String("|") +
                    QLatin1String("(") +
                      prefixes.join(QLatin1Char('|')) +
                    QLatin1String(")") +
                    filePattern +
                // Files with known full names
                QLatin1String("|") +
                    fullNames.join(QLatin1Char('|')) +
                QLatin1String(")")
    );
}

FileFilter::FileFilter(Session *session) :
    _session(session)
    , _dirPath(QString())
    , _currentFiles(QSet<QString>())
{
    QStringList patterns;
    QMimeDatabase mimeDatabase;
    const QList<QMimeType> allMimeTypes = mimeDatabase.allMimeTypes();
    for (const QMimeType &mimeType : allMimeTypes) {
        patterns.append(mimeType.globPatterns());
    }

    patterns.removeDuplicates();

    QString validFilename(QStringLiteral("[A-Za-z0-9\\._\\-]+"));
    QString pathRegex(QStringLiteral("([A-Za-z0-9\\._\\-/]+/)"));
    QString noSpaceRegex = QLatin1String("\\b") + createFileRegex(patterns, validFilename, pathRegex) + QLatin1String("\\b");

    QString spaceRegex = QLatin1String("'") + createFileRegex(patterns, validFilename, pathRegex) + QLatin1String("'");

    QString regex = QLatin1String("(") + noSpaceRegex + QLatin1String(")|(") + spaceRegex + QLatin1String(")");

    setRegExp(QRegularExpression(regex, QRegularExpression::DontCaptureOption));
}

FileFilter::HotSpot::~HotSpot()
{
    delete _fileObject;
}

QList<QAction *> FileFilter::HotSpot::actions()
{
    auto openAction = new QAction(_fileObject);
    openAction->setText(i18n("Open File"));
    QObject::connect(openAction, &QAction::triggered, _fileObject,
                     &Konsole::FilterObject::activated);
    QList<QAction *> actions;
    actions << openAction;
    return actions;
}
