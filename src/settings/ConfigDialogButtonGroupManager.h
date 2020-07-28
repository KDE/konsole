/*
    Copyright 2019 by Mariusz Glebocki <mglb@arccos-1.net>

    Based on KConfigDialog and KConfigDialogManager from KConfigWidgets

    Copyright (C) 2003 Benjamin C Meyer (ben+kdelibs at meyerhome dot net)
    Copyright (C) 2003 Waldo Bastian <bastian@kde.org>

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

#ifndef CONFIGDIALOGBUTTONGROUPMANAGER_H
#define CONFIGDIALOGBUTTONGROUPMANAGER_H

// Qt
#include <QObject>

// KDE
#include <KCoreConfigSkeleton>

class QString;
class QButtonGroup;
class QAbstractButton;
template <typename T> class QList;
template <class Key, class T> class QMap;

namespace Konsole
{
    // KConfigDialogManager-like class for managing QButtonGroups,
    // which are not supported by KConfigDialogManager yet. When
    // support will be available in minimum KF5 used by Konsole,
    // just remove this class and all expressions which refer to it.
    class ConfigDialogButtonGroupManager : public QObject
    {
        Q_OBJECT

    public:
        ConfigDialogButtonGroupManager(QObject *parent, KCoreConfigSkeleton *config);

        void addChildren(const QObject *parentObj);
        void add(const QButtonGroup *obj);
        bool hasChanged() const;
        bool isDefault() const;

    Q_SIGNALS:
        void settingsChanged();
        void widgetModified();

    public Q_SLOTS:
        void updateWidgets();
        void updateWidgetsDefault();
        void updateSettings();

    protected Q_SLOTS:
        void setButtonState(QAbstractButton *button, bool checked);

    private:
        // Returns configuration item associated with the group
        KCoreConfigSkeleton::ItemEnum * groupToConfigItemEnum(const QButtonGroup *group) const;

        // Returns a value the button represents in its group
        int buttonToEnumValue(const QAbstractButton *button) const;

        static const QString ManagedNamePrefix;

        mutable QMap<const QAbstractButton *, int> _buttonValues;
        KCoreConfigSkeleton *_config = nullptr;
        QList<const QButtonGroup *> _groups;
    };
}

#endif
