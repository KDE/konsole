/*
    SPDX-FileCopyrightText: 2019 Mariusz Glebocki <mglb@arccos-1.net>

    Based on KConfigDialog and KConfigDialogManager from KConfigWidgets

    SPDX-FileCopyrightText: 2003 Benjamin C Meyer (ben+kdelibs at meyerhome dot net)
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
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
template<typename T>
class QList;
template<class Key, class T>
class QMap;

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
    KCoreConfigSkeleton::ItemEnum *groupToConfigItemEnum(const QButtonGroup *group) const;

    // Returns a value the button represents in its group
    int buttonToEnumValue(const QAbstractButton *button) const;

    static const QString ManagedNamePrefix;

    mutable QMap<const QAbstractButton *, int> _buttonValues;
    KCoreConfigSkeleton *_config = nullptr;
    QList<const QButtonGroup *> _groups;
};
}

#endif
