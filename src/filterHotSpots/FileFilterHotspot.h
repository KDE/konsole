/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2020 Tomaz Canabrava <tcanabrava@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FILE_FILTER_HOTSPOT
#define FILE_FILTER_HOTSPOT

#include "RegExpFilterHotspot.h"

#include <QList>
#include <QPoint>
#include <QString>

#include <KFileItem>
#include <KFileItemActions>
#include <KIO/PreviewJob>

class QAction;
class QPixmap;
class QKeyEvent;
class QMouseEvent;

namespace Konsole
{
class Session;
class TerminalDisplay;

/**
 * Hotspot type created by FileFilter instances.
 */
class FileFilterHotSpot : public RegExpFilterHotSpot
{
    Q_OBJECT
public:
    FileFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn, const QStringList &capturedTexts, const QString &filePath, Session *session);
    ~FileFilterHotSpot() override;

    QList<QAction *> actions() override;

    /**
     * Opens kate for editing the file.
     */
    void activate(QObject *object = nullptr) override;
    QList<QAction *> setupMenu(QMenu *menu) override;

    KFileItem fileItem() const;

    void requestThumbnail(Qt::KeyboardModifiers modifiers, const QPoint &pos);
    void thumbnailRequested();

    bool hasDragOperation() const override;
    void startDrag() override;

    static void stopThumbnailGeneration();

    void mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev) override;
    void mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev) override;

    void keyPressEvent(TerminalDisplay *td, QKeyEvent *ev) override;

private:
    void openWithSysDefaultApp(const QString &filePath) const;
    void openWithEditorFromProfile(const QString &fullCmd, const QString &path) const;

    void showThumbnail(const KFileItem &item, const QPixmap &preview);
    QString _filePath;
    Session *_session = nullptr;

    // This is a pointer for performance reasons
    // constructing KFileItemActions is super expensive
    KFileItemActions *_menuActions = nullptr;

    QPoint _eventPos;
    QPoint _thumbnailPos;
    Qt::KeyboardModifiers _eventModifiers;
    bool _thumbnailFinished;

    /* This variable stores the pointer of the active HotSpot that
     * is generating the thumbnail now, so we can bail out early.
     * it's not used for pointer access.
     */
    static qintptr currentThumbnailHotspot;
    static bool _canGenerateThumbnail;
    static QPointer<KIO::PreviewJob> _previewJob;
};

}
#endif
