/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "FileFilterHotspot.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QDrag>
#include <QKeyEvent>
#include <QMenu>
#include <QMimeData>
#include <QMimeDatabase>
#include <QMouseEvent>
#include <QRegularExpression>
#include <QTimer>
#include <QToolTip>

#include <KIO/ApplicationLauncherJob>
#include <KIO/OpenUrlJob>

#include <KFileItemListProperties>
#include <KIO/JobUiDelegate>
#include <KLocalizedString>
#include <KMessageBox>
#include <KShell>

#include <kio_version.h>

#include "KonsoleSettings.h"
#include "konsoledebug.h"
#include "profile/Profile.h"
#include "session/SessionManager.h"
#include "terminalDisplay/TerminalDisplay.h"

using namespace Konsole;

FileFilterHotSpot::FileFilterHotSpot(int startLine,
                                     int startColumn,
                                     int endLine,
                                     int endColumn,
                                     const QStringList &capturedTexts,
                                     const QString &filePath,
                                     Session *session)
    : RegExpFilterHotSpot(startLine, startColumn, endLine, endColumn, capturedTexts)
    , _filePath(filePath)
    , _session(session)
    , _thumbnailFinished(false)
{
    setType(File);
}

void FileFilterHotSpot::activate(QObject *)
{
    if (!_session) { // The Session is dead, nothing to do
        return;
    }

    QString editorExecPath;
    int firstBlankIdx = -1;
    QString fullCmd;

    Profile::Ptr profile = SessionManager::instance()->sessionProfile(_session);
    const QString editorCmd = profile->textEditorCmd();
    if (!editorCmd.isEmpty()) {
        firstBlankIdx = editorCmd.indexOf(QLatin1Char(' '));
        if (firstBlankIdx != -1) {
            editorExecPath = QStandardPaths::findExecutable(editorCmd.mid(0, firstBlankIdx));
        } else { // No spaces, e.g. just a binary name "foo"
            editorExecPath = QStandardPaths::findExecutable(editorCmd);
        }
    }

    // Output of e.g.:
    // - grep with line numbers: "path/to/some/file:123:"
    //   grep with long lines e.g. "path/to/some/file:123:void blah" i.e. no space after 123:
    // - compiler errors with line/column numbers: "/path/to/file.cpp:123:123:"
    // - ctest failing unit tests: "/path/to/file(204)"
    static const QRegularExpression re(QStringLiteral(R"foo([:\(](\d+)(?:\)\])?(?::(\d+):|:[^\d]*)?$)foo"));
    const QRegularExpressionMatch match = re.match(_filePath);
    if (match.hasMatch()) {
        // The file path without the ":123" ... etc part
        const QString path = _filePath.mid(0, match.capturedStart(0));

        // TODO: show an error message to the user?
        if (editorExecPath.isEmpty()) { // Couldn't find the specified binary, fallback
            openWithSysDefaultApp(path);
            return;
        }
        if (firstBlankIdx != -1) {
            fullCmd = editorCmd;
            // Substitute e.g. "fooBinary" with full path, "/usr/bin/fooBinary"
            fullCmd.replace(0, firstBlankIdx, editorExecPath);

            fullCmd.replace(QLatin1String("PATH"), path);
            fullCmd.replace(QLatin1String("LINE"), match.captured(1));

            const QString col = match.captured(2);
            fullCmd.replace(QLatin1String("COLUMN"), !col.isEmpty() ? col : QLatin1String("0"));
        } else { // The editorCmd is just the binary name, no PATH, LINE or COLUMN
            // Add the "path" here, so it becomes "/path/to/fooBinary path"
            fullCmd += QLatin1Char(' ') + path;
        }

        openWithEditorFromProfile(fullCmd, path);
        return;
    }

    // There was no match, i.e. regular url "path/to/file"
    // Clean up the file path; the second branch in the regex is for "path/to/file:"
    QString path(_filePath);
    static const QRegularExpression cleanupRe(QStringLiteral(R"foo((:\d+[:]?|:)$)foo"), QRegularExpression::DontCaptureOption);
    path.remove(cleanupRe);
    if (!editorExecPath.isEmpty()) { // Use the editor from the profile settings
        const QString fCmd = editorExecPath + QLatin1Char(' ') + path;
        openWithEditorFromProfile(fCmd, path);
    } else { // Fallback
        openWithSysDefaultApp(path);
    }
}

void FileFilterHotSpot::openWithSysDefaultApp(const QString &filePath) const
{
    auto *job = new KIO::OpenUrlJob(QUrl::fromLocalFile(filePath));
    job->setUiDelegate(new KIO::JobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, QApplication::activeWindow()));
    job->setRunExecutables(false); // Always open scripts, shell/python/perl... etc, as text
    job->start();
}

void FileFilterHotSpot::openWithEditorFromProfile(const QString &fullCmd, const QString &path) const
{
    // Here we are mostly interested in text-based files, e.g. if it's a
    // PDF we should let the system default app open it.
    QMimeDatabase mdb;
    const auto mimeType = mdb.mimeTypeForFile(path);
    qCDebug(KonsoleDebug) << "FileFilterHotSpot: mime type for" << path << ":" << mimeType;

    if (!mimeType.inherits(QStringLiteral("text/plain"))) {
        openWithSysDefaultApp(path);
        return;
    }

    qCDebug(KonsoleDebug) << "fullCmd:" << fullCmd;

    KService::Ptr service(new KService(QString(), fullCmd, QString()));

    // ApplicationLauncherJob is better at reporting errors to the user than
    // CommandLauncherJob; no need to call job->setUrls() because the url is
    // already part of fullCmd
    auto *job = new KIO::ApplicationLauncherJob(service);
    connect(job, &KJob::result, this, [this, path, job]() {
        if (job->error() != 0) {
            // TODO: use KMessageWidget (like the "terminal is read-only" message)
            KMessageBox::sorry(QApplication::activeWindow(),
                               i18n("Could not open file with the text editor specified in the profile settings;\n"
                                    "it will be opened with the system default editor."));

            openWithSysDefaultApp(path);
        }
    });

    job->start();
}

FileFilterHotSpot::~FileFilterHotSpot() = default;

QList<QAction *> FileFilterHotSpot::actions()
{
    QAction *action = new QAction(i18n("Copy Location"), this);
    action->setIcon(QIcon::fromTheme(QStringLiteral("edit-copy-path")));
    connect(action, &QAction::triggered, this, [this] {
        QGuiApplication::clipboard()->setText(_filePath);
    });
    return {action};
}

QList<QAction *> FileFilterHotSpot::setupMenu(QMenu *menu)
{
    if (!_menuActions)
        _menuActions = new KFileItemActions;
    const QList<QAction *> currentActions = menu->actions();

    const KFileItem fileItem(QUrl::fromLocalFile(_filePath));
    const KFileItemList itemList({fileItem});
    const KFileItemListProperties itemProperties(itemList);
    _menuActions->setParent(this);
    _menuActions->setItemListProperties(itemProperties);
#if KIO_VERSION < QT_VERSION_CHECK(5, 82, 0)
    _menuActions->addOpenWithActionsTo(menu);

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
    auto *separator = new QAction(this);
    separator->setSeparator(true);
    menu->insertAction(firstAction, separator);
#else
    const QList<QAction *> actionList = menu->actions();
    _menuActions->insertOpenWithActionsTo(!actionList.isEmpty() ? actionList.at(0) : nullptr, menu, QStringList());
#endif

    QList<QAction *> addedActions = menu->actions();
    // addedActions will only contain the open-with actions
    for (auto *act : currentActions) {
        addedActions.removeOne(act);
    }

    return addedActions;
}

// Static variables for the HotSpot
bool FileFilterHotSpot::_canGenerateThumbnail = false;
QPointer<KIO::PreviewJob> FileFilterHotSpot::_previewJob;

void FileFilterHotSpot::requestThumbnail(Qt::KeyboardModifiers modifiers, const QPoint &pos)
{
    if (!KonsoleSettings::self()->enableThumbnails()) {
        return;
    }

    _canGenerateThumbnail = true;
    _eventModifiers = modifiers;
    _eventPos = pos;

    // Defer the real creation of the thumbnail by a few msec.
    QTimer::singleShot(250, this, [this] {
        thumbnailRequested();
    });
}

void FileFilterHotSpot::stopThumbnailGeneration()
{
    _canGenerateThumbnail = false;
    if (_previewJob != nullptr) {
        _previewJob->deleteLater();
        QToolTip::hideText();
    }
}

void FileFilterHotSpot::showThumbnail(const KFileItem &item, const QPixmap &preview)
{
    if (!_canGenerateThumbnail) {
        return;
    }
    _thumbnailFinished = true;
    Q_UNUSED(item)
    QByteArray data;
    QBuffer buffer(&data);
    preview.save(&buffer, "PNG", 100);

    const auto tooltipString = QStringLiteral("<img src='data:image/png;base64, %0'>").arg(QString::fromLocal8Bit(data.toBase64()));

    QToolTip::showText(_thumbnailPos, tooltipString, qApp->focusWidget());
}

void FileFilterHotSpot::thumbnailRequested()
{
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
    if (_previewJob != nullptr) {
        _previewJob->deleteLater();
    }

    _thumbnailFinished = false;

    // Show a "Loading" if Preview takes a long time.
    QTimer::singleShot(10, this, [this] {
        if (_previewJob == nullptr) {
            return;
        }
        if (!_thumbnailFinished) {
            QToolTip::showText(_thumbnailPos, i18n("Generating Thumbnail"), qApp->focusWidget());
        }
    });

    _previewJob = new KIO::PreviewJob(KFileItemList({fileItem()}), QSize(size, size));
    connect(_previewJob, &KIO::PreviewJob::gotPreview, this, &FileFilterHotSpot::showThumbnail);
    connect(_previewJob, &KIO::PreviewJob::failed, this, [] {
        qCDebug(KonsoleDebug) << "Error generating the preview" << _previewJob->errorString();
        QToolTip::hideText();
    });

    _previewJob->setAutoDelete(true);
    _previewJob->start();
}

KFileItem FileFilterHotSpot::fileItem() const
{
    return KFileItem(QUrl::fromLocalFile(_filePath));
}

void FileFilterHotSpot::mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    HotSpot::mouseEnterEvent(td, ev);
    requestThumbnail(ev->modifiers(), ev->globalPos());
}

void FileFilterHotSpot::mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev)
{
    HotSpot::mouseLeaveEvent(td, ev);
    stopThumbnailGeneration();
}

void FileFilterHotSpot::keyPressEvent(Konsole::TerminalDisplay *td, QKeyEvent *ev)
{
    HotSpot::keyPressEvent(td, ev);
    requestThumbnail(ev->modifiers(), QCursor::pos());
}

bool FileFilterHotSpot::hasDragOperation() const
{
    return true;
}

void FileFilterHotSpot::startDrag()
{
    auto *drag = new QDrag(this);
    auto *mimeData = new QMimeData();
    mimeData->setUrls({QUrl::fromLocalFile(_filePath)});

    drag->setMimeData(mimeData);
    drag->exec(Qt::CopyAction);
}
