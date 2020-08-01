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
#include "KonsoleSettings.h"

#include <algorithm>

// Qt
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QMimeDatabase>
#include <QString>
#include <QTextStream>
#include <QUrl>
#include <QMenu>
#include <QMouseEvent>
#include <QToolTip>
#include <QBuffer>
#include <QToolTip>
#include <QTimer>

// KDE
#include <KLocalizedString>
#include <KRun>
#include <KFileItem>
#include <KFileItemListProperties>
#include <KFileItemActions>
#include <KIO/PreviewJob>

// Konsole
#include "Session.h"
#include "TerminalCharacterDecoder.h"
#include "ProfileManager.h"
#include "SessionManager.h"

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

void Filter::HotSpot::setupMenu(QMenu *)
{

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
    switch(urlType()) {
    case Email:
        setType(EMailAddress);
        break;

    case StandardUrl:
    case Unknown:
    default:
        setType(Link);
    }
}

UrlFilter::HotSpot::UrlType UrlFilter::HotSpot::urlType() const
{
    const QString url = capturedTexts().at(0);

    // Don't use a ternary here, it gets completely unreadable
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
    auto match = std::find_if(_currentDirContents.cbegin(), _currentDirContents.cend(),
                              [filename](const QString &s) { return filename.startsWith(s); });

    if (match == _currentDirContents.cend()) {
        return nullptr;
    }

    return QSharedPointer<Filter::HotSpot>(new FileFilter::HotSpot(startLine, startColumn, endLine, endColumn, capturedTexts, _dirPath + filename));
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

FileFilter::HotSpot::~HotSpot() = default;

QList<QAction *> FileFilter::HotSpot::actions()
{
    QAction *action = new QAction(i18n("Copy Location"), this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy")));
    connect(action, &QAction::triggered, this, [this] {
        QGuiApplication::clipboard()->setText(_filePath);
    });
    return {action};
}

void FileFilter::HotSpot::setupMenu(QMenu *menu)
{
    // We are reusing the QMenu, but we need to update the actions anyhow.
    // Remove the 'Open with' actions from it, then add the new ones.
    QList<QAction*> toDelete;
    for (auto *action : menu->actions()) {
        if (action->text().toLower().remove(QLatin1Char('&')).contains(i18n("open with"))) {
            toDelete.append(action);
        }
    }
    qDeleteAll(toDelete);

    const KFileItem fileItem(QUrl::fromLocalFile(_filePath));
    const KFileItemList itemList({fileItem});
    const KFileItemListProperties itemProperties(itemList);
    _menuActions.setParent(this);
    _menuActions.setItemListProperties(itemProperties);
    _menuActions.addOpenWithActionsTo(menu);

    // Here we added the actions to the last part of the menu, but we need to move them up.
    // TODO: As soon as addOpenWithActionsTo accepts a index, change this.
    // https://bugs.kde.org/show_bug.cgi?id=423765
    QAction *firstAction = menu->actions().at(0);
    for (auto *action : menu->actions()) {
        if (action->text().toLower().remove(QLatin1Char('&')).contains(i18n("open with"))) {
            menu->removeAction(action);
            menu->insertAction(firstAction, action);
        }
    }
    QAction *separator = new QAction(this);
    separator->setSeparator(true);
    menu->insertAction(firstAction, separator);
}

// Static variables for the HotSpot
qintptr FileFilter::HotSpot::currentThumbnailHotspot = 0;
bool FileFilter::HotSpot::_canGenerateThumbnail = false;
QPointer<KIO::PreviewJob> FileFilter::HotSpot::_previewJob;

void FileFilter::HotSpot::requestThumbnail(Qt::KeyboardModifiers modifiers, const QPoint &pos) {
    _canGenerateThumbnail = true;
    currentThumbnailHotspot = reinterpret_cast<qintptr>(this);
    _eventModifiers = modifiers;
    _eventPos = pos;

    // Defer the real creation of the thumbnail by a few msec.
    QTimer::singleShot(250, this, [this]{
        if (currentThumbnailHotspot != reinterpret_cast<qintptr>(this)) {
            return;
        }

        thumbnailRequested();
    });
}

void FileFilter::HotSpot::stopThumbnailGeneration()
{
    _canGenerateThumbnail = false;
    if (_previewJob) {
        _previewJob->deleteLater();
        QToolTip::hideText();
    }
}

void Konsole::FileFilter::HotSpot::showThumbnail(const KFileItem& item, const QPixmap& preview)
{
    if (!_canGenerateThumbnail) {
        return;
    }
    _thumbnailFinished = true;
    Q_UNUSED(item)
    QByteArray data;
    QBuffer buffer(&data);
    preview.save(&buffer, "PNG", 100);

    const auto tooltipString = QStringLiteral("<img src='data:image/png;base64, %0'>")
        .arg(QString::fromLocal8Bit(data.toBase64()));

    QToolTip::showText(_thumbnailPos, tooltipString, qApp->focusWidget());
}

void FileFilter::HotSpot::thumbnailRequested() {
    if (!_canGenerateThumbnail) {
        return;
    }

    auto *settings = KonsoleSettings::self();

    Qt::KeyboardModifiers modifiers = settings->thumbnailCtrl() ? Qt::ControlModifier : Qt::NoModifier;
    modifiers |= settings->thumbnailAlt() ? Qt::AltModifier : Qt::NoModifier;
    modifiers |= settings->thumbnailShift() ? Qt::ShiftModifier : Qt::NoModifier;

    if (_eventModifiers != modifiers) {
        return;
    }

    _thumbnailPos = QPoint(_eventPos.x() + 100, _eventPos.y() - settings->thumbnailSize() / 2);

    const int size = KonsoleSettings::thumbnailSize();
    if (_previewJob) {
        _previewJob->deleteLater();
    }

    _thumbnailFinished = false;

    // Show a "Loading" if Preview takes a long time.
    QTimer::singleShot(10, this, [this]{
        if (!_previewJob) {
            return;
        }
        if (!_thumbnailFinished) {
            QToolTip::showText(_thumbnailPos, i18n("Generating Thumbnail"), qApp->focusWidget());
        }
    });

    _previewJob = new KIO::PreviewJob(KFileItemList({fileItem()}), QSize(size, size));
    connect(_previewJob, &KIO::PreviewJob::gotPreview, this, &FileFilter::HotSpot::showThumbnail);
    connect(_previewJob, &KIO::PreviewJob::failed, this, []{ QToolTip::hideText(); });
    _previewJob->setAutoDelete(true);
    _previewJob->start();
}

KFileItem Konsole::FileFilter::HotSpot::fileItem() const
{
    return KFileItem(QUrl::fromLocalFile(_filePath));
}
