/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef VIEWPROPERTIES_H
#define VIEWPROPERTIES_H

// Qt
#include <QColor>
#include <QHash>
#include <QIcon>
#include <QObject>
#include <QUrl>

// Konsole
#include "konsoleprivate_export.h"
#include "session/Session.h"

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
    explicit ViewProperties(QObject *parent);
    ~ViewProperties() override;

    /** Returns the icon associated with a view */
    QIcon icon() const;
    /** Returns the title associated with a view */
    QString title() const;
    /** Returns the color associated with a view */
    QColor color() const;

    /**
     * Returns the URL current associated with a view.
     * The default implementation returns an empty URL.
     */
    virtual QUrl url() const;

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
    virtual bool confirmClose() const
    {
        return true;
    }

    /** Finds a ViewProperties instance given its numeric identifier. */
    static ViewProperties *propertiesById(int id);

Q_SIGNALS:
    /** Emitted when the icon for a view changes */
    void iconChanged(ViewProperties *properties);
    /** Emitted when the title for a view changes */
    void titleChanged(ViewProperties *properties);
    /** Emitted when the color for a view changes */
    void colorChanged(ViewProperties *properties);
    /** Emitted when activity has occurred in this view. */
    void activity(ViewProperties *item);
    /** Emitted when notification for a view changes */
    void notificationChanged(ViewProperties *item, Session::Notification notification, bool enabled);
    /** Emitted when read only state changes */
    void readOnlyChanged(ViewProperties *item);
    /** Emitted when "copy input" state changes */
    void copyInputChanged(ViewProperties *item);

public Q_SLOTS:
    /**
     * Requests the renaming of this view.
     * The default implementation does nothing.
     */
    virtual void rename();

protected Q_SLOTS:
    /** Emits the activity() signal. */
    void fireActivity();

protected:
    /**
     * Subclasses may call this method to change the title.  This causes
     * a titleChanged() signal to be emitted
     */
    void setTitle(const QString &title);
    /**
     * Subclasses may call this method to change the icon.  This causes
     * an iconChanged() signal to be emitted
     */
    void setIcon(const QIcon &icon);
    /**
     * Subclasses may call this method to change the color.  This causes
     * a colorChanged() signal to be emitted
     */
    void setColor(const QColor &color);
    /** Subclasses may call this method to change the identifier. */
    void setIdentifier(int id);

private:
    Q_DISABLE_COPY(ViewProperties)

    QIcon _icon;
    QString _title;
    QColor _color;
    int _identifier;

    static QHash<int, ViewProperties *> _viewProperties;
};
}

#endif // VIEWPROPERTIES_H
