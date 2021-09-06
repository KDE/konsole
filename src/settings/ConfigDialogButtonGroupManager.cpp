/*
    SPDX-FileCopyrightText: 2019 Mariusz Glebocki <mglb@arccos-1.net>

    Based on KConfigDialog and KConfigDialogManager from KConfigWidgets

    SPDX-FileCopyrightText: 2003 Benjamin C Meyer (ben+kdelibs at meyerhome dot net)
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ConfigDialogButtonGroupManager.h"

// Qt
#include <QAbstractButton>
#include <QButtonGroup>
#include <QString>
#include <QTimer>

using namespace Konsole;

// const QString ConfigDialogButtonGroupManager::ManagedNamePrefix;
const QString ConfigDialogButtonGroupManager::ManagedNamePrefix = QStringLiteral("kcfg_");

ConfigDialogButtonGroupManager::ConfigDialogButtonGroupManager(QObject *parent, KCoreConfigSkeleton *config)
    : QObject(parent)
    , _config(config)
{
    Q_ASSERT(config);
    connect(_config, &KCoreConfigSkeleton::configChanged, this, &ConfigDialogButtonGroupManager::updateWidgets);
}

void ConfigDialogButtonGroupManager::addChildren(const QObject *parentObj)
{
    const auto allButtonGroups = parentObj->findChildren<QButtonGroup *>();
    for (const auto *buttonGroup : allButtonGroups) {
        if (buttonGroup->objectName().startsWith(ManagedNamePrefix)) {
            add(buttonGroup);
        }
    }
}

void ConfigDialogButtonGroupManager::add(const QButtonGroup *obj)
{
    Q_ASSERT(obj->exclusive());
    connect(obj,
            QOverload<QAbstractButton *, bool>::of(&QButtonGroup::buttonToggled),
            this,
            &ConfigDialogButtonGroupManager::setButtonState,
            Qt::UniqueConnection);
    _groups.append(obj);
}

bool ConfigDialogButtonGroupManager::hasChanged() const
{
    for (const QButtonGroup *group : qAsConst(_groups)) {
        if (group->checkedButton() == nullptr) {
            continue;
        }
        int value = buttonToEnumValue(group->checkedButton());
        const auto *enumItem = groupToConfigItemEnum(group);

        if (enumItem != nullptr && !enumItem->isEqual(value)) {
            return true;
        }
    }
    return false;
}

bool ConfigDialogButtonGroupManager::isDefault() const
{
    bool useDefaults = _config->useDefaults(true);
    bool result = !hasChanged();
    _config->useDefaults(useDefaults);
    return result;
}

void ConfigDialogButtonGroupManager::updateWidgets()
{
    bool prevSignalsBlocked = signalsBlocked();
    bool changed = false;
    blockSignals(true);
    for (const QButtonGroup *group : qAsConst(_groups)) {
        auto *enumItem = groupToConfigItemEnum(group);
        if (enumItem == nullptr) {
            continue;
        }

        int value = enumItem->value();
        const QString &valueName = enumItem->choices().at(value).name;
        QAbstractButton *currentButton = nullptr;
        for (auto &button : group->buttons()) {
            if (button->objectName() == valueName) {
                currentButton = button;
                break;
            }
        }
        if (currentButton == nullptr) {
            return;
        }
        currentButton->setChecked(true);
        changed = true;
    }
    blockSignals(prevSignalsBlocked);
    if (changed) {
        QTimer::singleShot(0, this, &ConfigDialogButtonGroupManager::widgetModified);
    }
}

void ConfigDialogButtonGroupManager::updateWidgetsDefault()
{
    bool useDefaults = _config->useDefaults(true);
    updateWidgets();
    _config->useDefaults(useDefaults);
}

void ConfigDialogButtonGroupManager::updateSettings()
{
    bool updateConfig = false;
    for (const QButtonGroup *group : qAsConst(_groups)) {
        auto *enumItem = groupToConfigItemEnum(group);
        if (enumItem == nullptr) {
            continue;
        }
        const auto *currentButton = group->checkedButton();
        if (currentButton == nullptr) {
            continue;
        }
        const int value = buttonToEnumValue(currentButton);
        if (value < 0) {
            continue;
        }

        if (!enumItem->isEqual(value)) {
            enumItem->setValue(value);
            updateConfig = true;
        }
    }
    if (updateConfig) {
        _config->save();
        Q_EMIT settingsChanged();
    }
}

void ConfigDialogButtonGroupManager::setButtonState(QAbstractButton *button, bool checked)
{
    Q_ASSERT(button->group());
    if (!checked) {
        // Both deselected and selected buttons trigger this slot, ignore the deselected one
        return;
    }
    auto *enumItem = groupToConfigItemEnum(button->group());
    if (enumItem == nullptr) {
        return;
    }

    int value = buttonToEnumValue(button);
    if (value < 0) {
        return;
    }

    Q_EMIT settingsChanged();
}

// Returns configuration item associated with the group
KCoreConfigSkeleton::ItemEnum *ConfigDialogButtonGroupManager::groupToConfigItemEnum(const QButtonGroup *group) const
{
    Q_ASSERT(group);
    const QString key = group->objectName().mid(ManagedNamePrefix.length());
    auto *item = _config->findItem(key);
    if (item == nullptr) {
        return nullptr;
    }
    auto *enumItem = dynamic_cast<KCoreConfigSkeleton::ItemEnum *>(item);
    if (enumItem == nullptr) {
        return nullptr;
    }
    return enumItem;
}

// Returns a value the button represents in its group
int ConfigDialogButtonGroupManager::buttonToEnumValue(const QAbstractButton *button) const
{
    Q_ASSERT(button);
    Q_ASSERT(button->group());

    if (_buttonValues.contains(button)) {
        return _buttonValues[button];
    }

    const auto *enumItem = groupToConfigItemEnum(button->group());
    if (enumItem == nullptr) {
        return -1;
    }
    const auto &choices = enumItem->choices();

    const QString buttonName = button->objectName();
    int value = -1;
    for (int i = 0; i < choices.size(); ++i) {
        if (buttonName == choices.at(i).name) {
            value = i;
            break;
        }
    }
    _buttonValues[button] = value;
    return value;
}
