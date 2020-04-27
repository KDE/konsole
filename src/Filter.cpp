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
    qDeleteAll(_filters);
}

void FilterChain::addFilter(Filter *filter)
{
    _filters.append(filter);
}

void FilterChain::removeFilter(Filter *filter)
{
    _filters.removeAll(filter);
}

void FilterChain::reset()
{
    for(auto *filter : _filters) {
        filter->reset();
    }
}

void FilterChain::setBuffer(const QString *buffer, const QList<int> *linePositions)
{
    for(auto *filter : _filters) {
        filter->setBuffer(buffer, linePositions);
    }
}

void FilterChain::process()
{
    for( auto *filter : _filters) {
        filter->process();
    }
}

void FilterChain::clear()
{
    _filters.clear();
}

QSharedPointer<Filter::HotSpot> FilterChain::hotSpotAt(int line, int column) const
{
    for(auto *filter : _filters) {
        QSharedPointer<Filter::HotSpot> spot = filter->hotSpotAt(line, column);
        if (spot != nullptr) {
           return spot;
        }
    }
    return nullptr;
}

QList<QSharedPointer<Filter::HotSpot>> FilterChain::hotSpots() const
{
    QList<QSharedPointer<Filter::HotSpot>> list;
    for (auto *filter : _filters) {
        list.append(filter->hotSpots());
   }
    return list;
}

TerminalImageFilterChain::TerminalImageFilterChain() :
    _buffer(nullptr),
    _linePositions(nullptr)
{
}

TerminalImageFilterChain::~TerminalImageFilterChain() = default;

void TerminalImageFilterChain::setImage(const Character * const image, int lines, int columns,
                                        const QVector<LineProperty> &lineProperties)
{
    if (_filters.empty()) {
        return;
    }

    // reset all filters and hotspots
    reset();

    PlainTextDecoder decoder;
    decoder.setLeadingWhitespace(true);
    decoder.setTrailingWhitespace(true);

    // setup new shared buffers for the filters to process on
    _buffer.reset(new QString());
    _linePositions.reset(new QList<int>());

    setBuffer(_buffer.get(), _linePositions.get());

    QTextStream lineStream(_buffer.get());
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
    _hotspotList.clear();
}

void Filter::setBuffer(const QString *buffer, const QList<int> *linePositions)
{
    _buffer = buffer;
    _linePositions = linePositions;
}

std::pair<int, int> Filter::getLineColumn(int position)
{
    Q_ASSERT(_linePositions);
    Q_ASSERT(_buffer);

    for (int i = 0; i < _linePositions->count(); i++) {
        const int nextLine = i == _linePositions->count() - 1
            ? _buffer->length() + 1
            : _linePositions->value(i + 1);

        if (_linePositions->value(i) <= position && position < nextLine) {
            return std::make_pair(i,  Character::stringWidth(buffer()->mid(_linePositions->value(i),
                                                     position - _linePositions->value(i))));
        }
    }
    return std::make_pair(-1, -1);
}

const QString *Filter::buffer()
{
    return _buffer;
}

Filter::HotSpot::~HotSpot() = default;

void Filter::addHotSpot(QSharedPointer<HotSpot> spot)
{
    _hotspotList << spot;

    for (int line = spot->startLine(); line <= spot->endLine(); line++) {
        _hotspots.insert(line, spot);
    }
}

QList<QSharedPointer<Filter::HotSpot>> Filter::hotSpots() const
{
    return _hotspotList;
}

QSharedPointer<Filter::HotSpot> Filter::hotSpotAt(int line, int column) const
{
    const auto hotspots = _hotspots.values(line);

    for (auto &spot : hotspots) {
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
    return {};
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
        std::pair<int, int> start = getLineColumn(match.capturedStart());
        std::pair<int, int> end = getLineColumn(match.capturedEnd());

        QSharedPointer<Filter::HotSpot> spot(
            newHotSpot(start.first, start.second,
                       end.first, end.second,
                       match.capturedTexts()
            )
        );

        if (spot == nullptr) {
            continue;
        }

        addHotSpot(spot);
    }
}

QSharedPointer<Filter::HotSpot> RegExpFilter::newHotSpot(int startLine, int startColumn, int endLine,
                                                int endColumn, const QStringList &capturedTexts)
{
    return QSharedPointer<Filter::HotSpot>(new RegExpFilter::HotSpot(startLine, startColumn,
                                     endLine, endColumn, capturedTexts));
}

QSharedPointer<Filter::HotSpot> UrlFilter::newHotSpot(int startLine, int startColumn, int endLine,
                                             int endColumn, const QStringList &capturedTexts)
{
    return QSharedPointer<Filter::HotSpot>(new UrlFilter::HotSpot(startLine, startColumn,
                                  endLine, endColumn, capturedTexts));
}

UrlFilter::HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn,
                            const QStringList &capturedTexts) :
    RegExpFilter::HotSpot(startLine, startColumn, endLine, endColumn, capturedTexts)
{
    setType(Link);
}

UrlFilter::HotSpot::UrlType UrlFilter::HotSpot::urlType() const
{
    const QString url = capturedTexts().at(0);
    return FullUrlRegExp.match(url).hasMatch() ? StandardUrl
         : EmailAddressRegExp.match(url).hasMatch() ? Email
         : Unknown;
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
            // prepend https:// ( eg. "www.kde.org" --> "https://www.kde.org" )
            if (!url.contains(QLatin1String("://"))) {
                url.prepend(QLatin1String("https://"));
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

UrlFilter::HotSpot::~HotSpot() = default;

QList<QAction *> UrlFilter::HotSpot::actions()
{
    auto openAction = new QAction(this);
    auto copyAction = new QAction(this);

    const UrlType kind = urlType();
    Q_ASSERT(kind == StandardUrl || kind == Email);

    if (kind == StandardUrl) {
        openAction->setText(i18n("Open Link"));
        openAction->setIcon(QIcon::fromTheme(QStringLiteral("internet-services")));
        copyAction->setText(i18n("Copy Link Address"));
        copyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-url")));
    } else if (kind == Email) {
        openAction->setText(i18n("Send Email To..."));
        openAction->setIcon(QIcon::fromTheme(QStringLiteral("mail-send")));
        copyAction->setText(i18n("Copy Email Address"));
        copyAction->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-mail")));
    }

    // object names are set here so that the hotspot performs the
    // correct action when activated() is called with the triggered
    // action passed as a parameter.
    openAction->setObjectName(QStringLiteral("open-action"));
    copyAction->setObjectName(QStringLiteral("copy-action"));

    QObject::connect(openAction, &QAction::triggered, this, [this, openAction]{ activate(openAction); });
    QObject::connect(copyAction, &QAction::triggered, this, [this, copyAction]{ activate(copyAction); });

    return {openAction, copyAction};
}

/**
  * File Filter - Construct a filter that works on local file paths using the
  * posix portable filename character set combined with KDE's mimetype filename
  * extension blob patterns.
  * https://pubs.opengroup.org/onlinepubs/9699919799/basedefs/V1_chap03.html#tag_03_267
  */

QSharedPointer<Filter::HotSpot> FileFilter::newHotSpot(int startLine, int startColumn, int endLine,
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

    // Return nullptr if it's not:
    // <current dir>/filename
    // <current dir>/childDir/filename
    bool isChild = false;
    for (const QString &s : _currentDirContents) {
        if (filename.startsWith(s)) {
            isChild = true;
            break;
        }
    }
    if (!isChild) {
        return nullptr;
    }

    return QSharedPointer<Filter::HotSpot>(new FileFilter::HotSpot(startLine, startColumn, endLine, endColumn, capturedTexts, _dirPath + filename));
}

void FileFilter::process()
{
    const QDir dir(_session->currentWorkingDirectory());
    _dirPath = dir.canonicalPath() + QLatin1Char('/');
    _currentDirContents = dir.entryList(QDir::Dirs | QDir::Files);

    RegExpFilter::process();
}

FileFilter::HotSpot::HotSpot(int startLine, int startColumn, int endLine, int endColumn,
                             const QStringList &capturedTexts, const QString &filePath) :
    RegExpFilter::HotSpot(startLine, startColumn, endLine, endColumn, capturedTexts),
    _filePath(filePath)
{
    setType(Link);
}

void FileFilter::HotSpot::activate(QObject *)
{
    new KRun(QUrl::fromLocalFile(_filePath), QApplication::activeWindow());
}

QString createFileRegex(const QStringList &patterns, const QString &filePattern, const QString &pathPattern)
{
    QStringList suffixes = patterns.filter(QRegularExpression(QStringLiteral("^\\*") + filePattern + QStringLiteral("$")));
    QStringList prefixes = patterns.filter(QRegularExpression(QStringLiteral("^") + filePattern + QStringLiteral("+\\*$")));
    const QStringList fullNames = patterns.filter(QRegularExpression(QStringLiteral("^") + filePattern + QStringLiteral("$")));


    suffixes.replaceInStrings(QStringLiteral("*"), QString());
    suffixes.replaceInStrings(QStringLiteral("."), QStringLiteral("\\."));
    prefixes.replaceInStrings(QStringLiteral("*"), QString());
    prefixes.replaceInStrings(QStringLiteral("."), QStringLiteral("\\."));

    return QString(
        // Optional path in front
        pathPattern + QLatin1Char('?')
        + QLatin1Char('(')
        // Files with known suffixes, e.g. "[A-Za-z0-9\\._\\-]+(txt|cpp|h|xml)"
        + filePattern + QLatin1Char('(') + suffixes.join(QLatin1Char('|')) + QLatin1Char(')')
        + QLatin1Char('|')
        // Files with known prefixes, e.g. "(Makefile\\.)[A-Za-z0-9\\._\\-]+" to match "Makefile.am"
        + QLatin1Char('(') + prefixes.join(QLatin1Char('|')) + QLatin1Char(')') + filePattern
        + QLatin1Char('|')
        // Files with known full names, e.g. "ChangeLog|COPYING"
        + fullNames.join(QLatin1Char('|'))
        + QLatin1Char(')')
    );
}

FileFilter::FileFilter(Session *session) :
    _session(session)
    , _dirPath(QString())
    , _currentDirContents(QStringList())
{
    static QRegularExpression re = QRegularExpression(QString(), QRegularExpression::DontCaptureOption);
    if (re.pattern().isEmpty()) {
        QStringList patterns;
        QMimeDatabase mimeDatabase;
        const QList<QMimeType> allMimeTypes = mimeDatabase.allMimeTypes();
        for (const QMimeType &mimeType : allMimeTypes) {
            patterns.append(mimeType.globPatterns());
        }

        patterns.removeDuplicates();

        const QString fileRegex = createFileRegex(patterns,
                                                  QStringLiteral("[A-Za-z0-9\\._\\-]+"),    // filenames regex
                                                  QStringLiteral("([A-Za-z0-9\\._\\-/]+/)") // path regex
                                                 );

        const QString regex = QLatin1String("(\\b") + fileRegex + QLatin1String("\\b)") // file names with no spaces
                              + QLatin1Char('|')
                              + QLatin1String("('") + fileRegex + QLatin1String("')");  // file names with spaces

        re.setPattern(regex);
    }

    setRegExp(re);
}

FileFilter::HotSpot::~HotSpot() = default;

QList<QAction *> FileFilter::HotSpot::actions()
{
    auto openAction = new QAction(this);
    openAction->setText(i18n("Open File"));
    QObject::connect(openAction, &QAction::triggered, this, [this ]{ activate(); });
    return {openAction};
}
