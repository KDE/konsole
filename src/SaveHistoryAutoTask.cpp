/*
    SPDX-FileCopyrightText: 2024 Theodore Wang <theodorewang12@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "SaveHistoryAutoTask.h"

#include <QApplication>
#include <QFileDialog>
#include <QLockFile>
#include <QTextStream>

#include <QDebug>

#include <KConfig>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KMessageBox>
#include <KSharedConfig>

#include "Emulation.h"
#include "session/SessionManager.h"

namespace Konsole
{
QString SaveHistoryAutoTask::_saveDialogRecentURL;

SaveHistoryAutoTask::SaveHistoryAutoTask(QObject *parent)
    : SessionTask(parent)
    , _droppedBytes(0)
    , _bytesLines(0)
    , _pendingChanges(false)
{
}

SaveHistoryAutoTask::~SaveHistoryAutoTask() = default;

bool SaveHistoryAutoTask::execute()
{
    QFileDialog *dialog = new QFileDialog(QApplication::activeWindow());
    dialog->setAcceptMode(QFileDialog::AcceptSave);

    QStringList mimeTypes{QStringLiteral("text/plain")};
    dialog->setMimeTypeFilters(mimeTypes);

    KSharedConfigPtr konsoleConfig = KSharedConfig::openConfig();
    auto group = konsoleConfig->group(QStringLiteral("SaveHistory Settings"));

    if (_saveDialogRecentURL.isEmpty()) {
        const auto list = group.readPathEntry("Recent URLs", QStringList());
        if (list.isEmpty()) {
            dialog->setDirectory(QDir::homePath());
        } else {
            dialog->setDirectoryUrl(QUrl(list.at(0)));
        }
    } else {
        dialog->setDirectoryUrl(QUrl(_saveDialogRecentURL));
    }

    Q_ASSERT(sessions().size() == 1);

    dialog->setWindowTitle(i18n("Save Output From %1", session()->title(Session::NameRole)));

    int result = dialog->exec();

    if (result != QDialog::Accepted) {
        return false;
    }

    QUrl url = (dialog->selectedUrls()).at(0);

    if (!url.isValid()) {
        // UI:  Can we make this friendlier?
        KMessageBox::error(nullptr, i18n("%1 is an invalid URL, the output could not be saved.", url.url()));
        return false;
    }

    // Save selected URL for next time
    _saveDialogRecentURL = url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toString();
    group.writePathEntry("Recent URLs", _saveDialogRecentURL);

    const QString path = url.path();

    _destinationFile.setFileName(path);

    if (!_destinationFile.open(QFile::ReadWrite)) {
        KMessageBox::error(nullptr, i18n("Failed to create autosave file at %1.", url.url()));
        return false;
    }

    _watcher.addPath(path);

    connect(&_watcher, &QFileSystemWatcher::fileChanged, this, &SaveHistoryAutoTask::fileModified);
    connect(&_timer, &QTimer::timeout, this, &SaveHistoryAutoTask::linesChanged);
    connect(session()->emulation(), &Emulation::outputChanged, this, [&]() {
        _pendingChanges = true;
    });
    connect(session()->emulation(), &Emulation::imageSizeChanged, this, &SaveHistoryAutoTask::imageResized);
    connect(session()->emulation(), &Emulation::updateDroppedLines, this, &SaveHistoryAutoTask::linesDropped);
    connect(session()->emulation(), &Emulation::primaryScreenInUse, this, [&]() {
        KMessageBox::error(nullptr, i18n("Stopping autosave due to switching of screens."));
        stop();
    });

    _timer.setSingleShot(true);

    readLines();
    dialog->deleteLater();
    return true;
}

void SaveHistoryAutoTask::fileModified()
{
    stop();
    KMessageBox::error(nullptr, i18n("Autosave file has been modified externally, preventing further autosaves."));
}

void SaveHistoryAutoTask::linesDropped(int linesDropped)
{
    if (linesDropped > 0) {
        if (linesDropped > _bytesLines.count()) {
            readLines();
        }

        if (linesDropped == _bytesLines.size()) {
            _droppedBytes = _destinationFile.size();
        } else {
            _droppedBytes = _bytesLines[linesDropped];
        }

        for (int i = 0; i < linesDropped; ++i) {
            _bytesLines.pop_front();
        }
    }
}

void SaveHistoryAutoTask::stop()
{
    Q_EMIT completed(true);
    disconnect();

    if (autoDelete()) {
        deleteLater();
    }
}

void SaveHistoryAutoTask::imageResized(int /*rows*/, int /*columns*/)
{
    updateByteLineAnchors();
}

void SaveHistoryAutoTask::linesChanged()
{
    if (_pendingChanges) {
        readLines();
    } else {
        _timer.start(timerInterval());
    }
}

void SaveHistoryAutoTask::readLines()
{
    if (session().isNull()) {
        stop();
        return;
    }

    _timer.stop();

    if (!updateArchive()) {
        stop();
        KMessageBox::error(nullptr, i18n("Failed to update autosave state on output changes."));
        return;
    }

    updateByteLineAnchors();

    _pendingChanges = false;
    _timer.start(timerInterval());
}

bool SaveHistoryAutoTask::updateArchive()
{
    _watcher.removePath(_destinationFile.fileName());

    QTextStream stream(&_destinationFile);

    if (!_destinationFile.resize(_droppedBytes) || !stream.seek(_destinationFile.size())) {
        return false;
    }

    _decoder.begin(&stream);
    session()->emulation()->writeToStream(&_decoder, 0, session()->emulation()->lineCount() - 1);
    _decoder.end();
    stream.flush();

    if (stream.status() != QTextStream::Ok) {
        return false;
    }

    _watcher.addPath(_destinationFile.fileName());

    return true;
}

void SaveHistoryAutoTask::updateByteLineAnchors()
{
    if (session().isNull()) {
        stop();
        return;
    }

    qint64 currentByte = _droppedBytes;
    QList<int> lineLengths = session()->emulation()->getCurrentScreenCharacterCounts();

    _bytesLines.resize(lineLengths.size());
    _destinationFile.seek(currentByte);

    for (int i = 0; i < lineLengths.size(); ++i) {
        _bytesLines[i] = currentByte;
        QString line;

        int lineNumChars = lineLengths[i];

        for (int j = 0; j < lineNumChars; ++j) {
            char c;

            if (!_destinationFile.getChar(&c)) {
                qDebug() << "LINE " << (i + 1) << " (" << lineNumChars << ") : " << line;
                qDebug() << "INVALID";
                return;
            }

            line.append(QLatin1Char(c));
        }

        currentByte = _destinationFile.pos();
        qDebug() << "LINE " << (i + 1) << " (" << lineNumChars << ") : " << line;
    }

    if (_destinationFile.atEnd())
        qDebug() << "VALID";
    else
        qDebug() << "INVALID";
}

const QPointer<Session> &SaveHistoryAutoTask::session() const
{
    return sessions()[0];
}

int SaveHistoryAutoTask::timerInterval() const
{
    return SessionManager::instance()->sessionProfile(session().data())->property<int>(Profile::AutoSaveInterval);
}

}

#include "moc_SaveHistoryAutoTask.cpp"
