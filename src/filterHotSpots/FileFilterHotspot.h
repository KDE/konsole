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

#ifndef FILE_FILTER_HOTSPOT
#define FILE_FILTER_HOTSPOT

#include "RegExpFilterHotspot.h"

#include <QList>
#include <QString>
#include <QPoint>

#include <KFileItemActions>
#include <KFileItem>
#include <KIO/PreviewJob>

class QAction;
class QPixmap;
class QKeyEvent;
class QMouseEvent;

namespace Konsole {
class TerminalDisplay;

/**
* Hotspot type created by FileFilter instances.
*/
class FileFilterHotSpot : public RegExpFilterHotSpot
{
public:
    FileFilterHotSpot(int startLine, int startColumn, int endLine, int endColumn,
            const QStringList &capturedTexts, const QString &filePath);
    ~FileFilterHotSpot() override;

    QList<QAction *> actions() override;

    /**
        * Opens kate for editing the file.
        */
    void activate(QObject *object = nullptr) override;
    void setupMenu(QMenu *menu) override;

    KFileItem fileItem() const;
    void requestThumbnail(Qt::KeyboardModifiers modifiers, const QPoint &pos);
    void thumbnailRequested();

    static void stopThumbnailGeneration();

    void mouseEnterEvent(TerminalDisplay *td, QMouseEvent *ev) override;
    void mouseLeaveEvent(TerminalDisplay *td, QMouseEvent *ev) override;

    void keyPressEvent(TerminalDisplay *td, QKeyEvent *ev) override;

private:
    void showThumbnail(const KFileItem& item, const QPixmap& preview);
    QString _filePath;
    KFileItemActions _menuActions;

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
