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

#ifndef VIEWPROPERTIES_H
#define VIEWPROPERTIES_H

// Qt
#include <QtGui/QIcon>
#include <QtCore/QObject>
#include <QtCore/QHash>
#include <QtCore/QMimeData>

// KDE
#include <KUrl>

// Konsole
#include "konsole_export.h"

namespace Konsole
{
/**
 * Encapsulates user-visible information about the terminal session currently being displayed in a view,
 * such as the associated title and icon.
 *
 * This can be used by navigation widgets in a ViewContainer sub-class to provide a tab, label or other
 * item for switching between views.
 */
class KONSOLEPRIVATE_EXPORT ViewProperties : public QObject
{
    Q_OBJECT

public:
    explicit ViewProperties(QObject* parent);
    virtual ~ViewProperties();

    /** Returns the icon associated with a view */
    QIcon icon() const;
    /** Returns the title associated with a view */
    QString title() const;

    /**
     * Returns the URL current associated with a view.
     * The default implementation returns an empty URL.
     */
    virtual KUrl url() const;

    /**
     * Returns the current directory associated with a view.
     * This may be the same as url()
     * The default implementation returns an empty string.
     */
    virtual QString currentDir() const;

    /**
     * A unique identifier associated with this
     * ViewProperties instance.
     */
    int identifier() const;

    /**
     * Sub-classes may re-implement this method to display a message to the user
     * to allow them to confirm whether to close a view.
     * The default implementation always returns true
     */
    virtual bool confirmClose() const {
        return true;
    }

    /** Finds a ViewProperties instance given its numeric identifier. */
    static ViewProperties* propertiesById(int id);

    /** Name of mime format to use in drag-and-drop operations. */
    static QString mimeType() {
        return _mimeType;
    }

    /** Returns a new QMimeData instance which represents the view with the given @p id
     * (See identifier()).  The QMimeData instance returned must be deleted by the caller.
     */
    static QMimeData* createMimeData(int id) {
        QMimeData* mimeData = new QMimeData;
        QByteArray data((char*)&id, sizeof(int));
        mimeData->setData(mimeType(), data);
        return mimeData;
    }
    /** Decodes a QMimeData instance created with createMimeData() and returns the identifier
     * of the associated view.  The associated ViewProperties instance can then be retrieved by
     * calling propertiesById()
     *
     * The QMimeData instance must support the mime format returned by mimeType()
     */
    static int decodeMimeData(const QMimeData* mimeData) {
        return *(int*)(mimeData->data(ViewProperties::mimeType()).constData());
    }

signals:
    /** Emitted when the icon for a view changes */
    void iconChanged(ViewProperties* properties);
    /** Emitted when the title for a view changes */
    void titleChanged(ViewProperties* properties);
    /** Emitted when activity has occurred in this view. */
    void activity(ViewProperties* item);

public slots:
    /**
     * Requests the renaming of this view.
     * The default implementation does nothing.
     */
    virtual void rename();

protected slots:
    /** Emits the activity() signal. */
    void fireActivity();

protected:
    /**
     * Subclasses may call this method to change the title.  This causes
     * a titleChanged() signal to be emitted
     */
    void setTitle(const QString& title);
    /**
     * Subclasses may call this method to change the icon.  This causes
     * an iconChanged() signal to be emitted
     */
    void setIcon(const QIcon& icon);
    /** Subclasses may call this method to change the identifier. */
    void setIdentifier(int id);
private:
    QIcon _icon;
    QString _title;
    int _id;

    static QHash<int, ViewProperties*> _viewProperties;
    static QString _mimeType;
};
}

#endif //VIEWPROPERTIES_H
