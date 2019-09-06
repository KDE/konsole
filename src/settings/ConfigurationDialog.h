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

#ifndef CONFIGURATIONDIALOG_H
#define CONFIGURATIONDIALOG_H

// Qt
#include <QButtonGroup>
#include <QAbstractButton>
#include <QTimer>
#include <QMap>

// KDE
#include <KPageDialog>
#include <KCoreConfigSkeleton>

// Konsole
#include "konsoleprivate_export.h"

class QWidget;
class KConfigDialogManager;

namespace Konsole {

class ConfigDialogButtonGroupManager;

// KConfigDialog-like class, as the original KConfigDialog wraps
// all pages in QScrollArea. KConfigDialog, when fixed, should
// be source compatible with this class, so simple class replace
// should suffice.
class KONSOLEPRIVATE_EXPORT ConfigurationDialog: public KPageDialog
{
    Q_OBJECT

Q_SIGNALS:
    void widgetModified();
    void settingsChanged();

public:
    explicit ConfigurationDialog(QWidget *parent, KCoreConfigSkeleton *config);
    ~ConfigurationDialog() override = default;

    void addPage(KPageWidgetItem *item, bool manage);

protected Q_SLOTS:
    void updateButtons();
    void settingsChangedSlot();

protected:
    void setApplyButtonEnabled(bool enabled);
    void setRestoreDefaultsButtonEnabled(bool enabled);
    void showEvent(QShowEvent *event) override;

private:
    Q_DISABLE_COPY(ConfigurationDialog)

    KConfigDialogManager *_manager = nullptr;
    ConfigDialogButtonGroupManager *_groupManager = nullptr;
    bool _shown = false;
};

// KConfigDialogManager-like class for managing QButtonGroups,
// which are not supported by KConfigDialogManager yet. When
// support will be available in minimum KF5 used by Konsole,
// just remove this class and all expressions which refer to it.
class ConfigDialogButtonGroupManager: public QObject
{
    Q_OBJECT

public:
    ConfigDialogButtonGroupManager(QObject *parent, KCoreConfigSkeleton *config)
        : QObject(parent)
        , _config(config)
    {
        Q_ASSERT(config);
        connect(_config, &KCoreConfigSkeleton::configChanged,
                this, &ConfigDialogButtonGroupManager::updateWidgets);
    }

    void addChildren(const QObject *parentObj)
    {
        const auto allButtonGroups = parentObj->findChildren<QButtonGroup *>();
        for (const auto *buttonGroup: allButtonGroups) {
            if (buttonGroup->objectName().startsWith(ManagedNamePrefix)) {
                add(buttonGroup);
            }
        }
    }
    void add(const QButtonGroup *obj)
    {
        Q_ASSERT(obj->exclusive());
        connect(obj, QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled),
                this, &ConfigDialogButtonGroupManager::setButtonState, Qt::UniqueConnection);
        _groups.append(obj);

    }

    bool hasChanged() const {
        for(const QButtonGroup *group: qAsConst(_groups)) {
            int value = buttonToEnumValue(group->checkedButton());
            const auto enumItem = groupToConfigItemEnum(group);

            if(!enumItem->isEqual(value)) {
                return true;
            }
        }
        return false;
    }

    bool isDefault() const {
        bool useDefaults = _config->useDefaults(true);
        bool result = !hasChanged();
        _config->useDefaults(useDefaults);
        return result;
    }

Q_SIGNALS:
    void settingsChanged();
    void widgetModified();

public Q_SLOTS:
    void updateWidgets()
    {
        bool prevSignalsBlocked = signalsBlocked();
        bool changed = false;
        blockSignals(true);
        for(const QButtonGroup *group: qAsConst(_groups)) {
            auto *enumItem = groupToConfigItemEnum(group);
            if(enumItem == nullptr) {
                continue;
            }

            int value = enumItem->value();
            const QString &valueName = enumItem->choices().at(value).name;
            QAbstractButton *currentButton = nullptr;
            for(auto &button: group->buttons()) {
                if(button->objectName() == valueName) {
                    currentButton = button;
                    break;
                }
            }
            if(currentButton == nullptr) {
                return;
            }
            currentButton->setChecked(true);
            changed = true;
        }
        blockSignals(prevSignalsBlocked);
        if(changed) {
            QTimer::singleShot(0, this, &ConfigDialogButtonGroupManager::widgetModified);
        }
    }

    void updateWidgetsDefault() {
        bool useDefaults = _config->useDefaults(true);
        updateWidgets();
        _config->useDefaults(useDefaults);
    }

    void updateSettings() {
        bool updateConfig = false;
        for(const QButtonGroup *group: qAsConst(_groups)) {
            auto *enumItem = groupToConfigItemEnum(group);
            if(enumItem == nullptr) {
                continue;
            }
            const auto *currentButton = group->checkedButton();
            if(currentButton == nullptr) {
                continue;
            }
            const int value = buttonToEnumValue(currentButton);
            if(value < 0) {
                continue;
            }

            if(!enumItem->isEqual(value)) {
                enumItem->setValue(value);
                updateConfig = true;
            }
        }
        if(updateConfig) {
            _config->save();
            emit settingsChanged();
        }
    }

protected Q_SLOTS:
    void setButtonState(QAbstractButton *button, bool checked)
    {
        Q_ASSERT(button);
        Q_ASSERT(button->group());
        if(!checked) {
            // Both deselected and selected buttons trigger this slot, ignore the deselected one
            return;
        }
        auto *enumItem = groupToConfigItemEnum(button->group());
        if(enumItem == nullptr) {
            return;
        }

        int value = buttonToEnumValue(button);
        if(value < 0) {
            return;
        }

        emit settingsChanged();
    }

private:
    // Returns configuration item associated with the group
    KCoreConfigSkeleton::ItemEnum * groupToConfigItemEnum(const QButtonGroup *group) const {
        Q_ASSERT(group);
        const QString key = group->objectName().mid(ManagedNamePrefix.length());
        auto *item = _config->findItem(key);
        if(item == nullptr) {
            return nullptr;
        }
        auto *enumItem = dynamic_cast<KCoreConfigSkeleton::ItemEnum *>(item);
        if(enumItem == nullptr) {
            return nullptr;
        }
        return enumItem;
    }

    // Returns a value the button represents in its group
    int buttonToEnumValue(const QAbstractButton *button) const {
        Q_ASSERT(button);
        Q_ASSERT(button->group());

        if(_buttonValues.contains(button)) {
            return _buttonValues[button];
        }

        const auto *enumItem = groupToConfigItemEnum(button->group());
        if(enumItem == nullptr) {
            return -1;
        }
        const auto &choices = enumItem->choices();

        const QString buttonName = button->objectName();
        int value = -1;
        for(int i = 0; i < choices.size(); ++i) {
            if(buttonName == choices.at(i).name) {
                value = i;
                break;
            }
        }
        _buttonValues[button] = value;
        return value;
    }

    static const QString ManagedNamePrefix;

    mutable QMap<const QAbstractButton *, int> _buttonValues;
    KCoreConfigSkeleton *_config = nullptr;
    QList<const QButtonGroup *> _groups;
};

}

#endif // CONFIGURATIONDIALOG_H
