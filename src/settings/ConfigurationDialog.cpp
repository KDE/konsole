/*
    SPDX-FileCopyrightText: 2019 Mariusz Glebocki <mglb@arccos-1.net>

    Based on KConfigDialog from KConfigWidgets

    SPDX-FileCopyrightText: 2003 Benjamin C Meyer (ben+kdelibs at meyerhome dot net)
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ConfigurationDialog.h"

// Konsole
#include "ConfigDialogButtonGroupManager.h"

// Qt
#include <QDialogButtonBox>
#include <QPushButton>

// KDE
#include <KConfigDialogManager>
#include <KLocalizedString>

using namespace Konsole;

ConfigurationDialog::ConfigurationDialog(QWidget *parent, KCoreConfigSkeleton *config)
    : KPageDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Configure"));
    // Setting this after modifying buttonBox results in initial focus set to buttonBox.
    setFaceType(KPageDialog::List);

    buttonBox()->setStandardButtons(QDialogButtonBox::RestoreDefaults | QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel);
    connect(buttonBox()->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, this, &ConfigurationDialog::updateButtons);
    connect(buttonBox()->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, this, &ConfigurationDialog::updateButtons);

    _manager = new KConfigDialogManager(this, config);

    connect(_manager, QOverload<>::of(&KConfigDialogManager::settingsChanged), this, &ConfigurationDialog::settingsChangedSlot);

    connect(_manager, &KConfigDialogManager::widgetModified, this, &ConfigurationDialog::updateButtons);

    connect(buttonBox()->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, _manager, &KConfigDialogManager::updateSettings);
    connect(buttonBox()->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, _manager, &KConfigDialogManager::updateSettings);
    connect(buttonBox()->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, _manager, &KConfigDialogManager::updateWidgets);
    connect(buttonBox()->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked, _manager, &KConfigDialogManager::updateWidgetsDefault);

    _groupManager = new ConfigDialogButtonGroupManager(this, config);
    connect(_groupManager, &ConfigDialogButtonGroupManager::settingsChanged, this, &ConfigurationDialog::settingsChangedSlot);
    connect(_groupManager, &ConfigDialogButtonGroupManager::widgetModified, this, &ConfigurationDialog::updateButtons);

    connect(buttonBox()->button(QDialogButtonBox::Ok), &QAbstractButton::clicked, _groupManager, &ConfigDialogButtonGroupManager::updateSettings);
    connect(buttonBox()->button(QDialogButtonBox::Apply), &QAbstractButton::clicked, _groupManager, &ConfigDialogButtonGroupManager::updateSettings);
    connect(buttonBox()->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked, _groupManager, &ConfigDialogButtonGroupManager::updateWidgets);
    connect(buttonBox()->button(QDialogButtonBox::RestoreDefaults),
            &QAbstractButton::clicked,
            _groupManager,
            &ConfigDialogButtonGroupManager::updateWidgetsDefault);

    setApplyButtonEnabled(false);
}

void ConfigurationDialog::addPage(KPageWidgetItem *item, bool manage)
{
    Q_ASSERT(item);
    Q_ASSERT(item->widget());

    KPageDialog::addPage(item);

    item->setParent(this);

    if (manage) {
        _manager->addWidget(item->widget());
        _groupManager->addChildren(item->widget());
    }

    if (_shown && manage) {
        QPushButton *defaultButton = buttonBox()->button(QDialogButtonBox::RestoreDefaults);
        if (defaultButton != nullptr) {
            bool isDefault = defaultButton->isEnabled() && _manager->isDefault();
            defaultButton->setEnabled(!isDefault);
        }
    }
}

void ConfigurationDialog::updateButtons()
{
    static bool onlyOnce = false;
    if (onlyOnce) {
        return;
    }
    onlyOnce = true;

    bool has_changed = _manager->hasChanged() || _groupManager->hasChanged();
    setApplyButtonEnabled(has_changed);

    bool is_default = _manager->isDefault() && _groupManager->isDefault();
    setRestoreDefaultsButtonEnabled(!is_default);

    Q_EMIT widgetModified();
    onlyOnce = false;
}

void ConfigurationDialog::settingsChangedSlot()
{
    updateButtons();
    Q_EMIT settingsChanged();
}

void ConfigurationDialog::setApplyButtonEnabled(bool enabled)
{
    QPushButton *applyButton = buttonBox()->button(QDialogButtonBox::Apply);
    if (applyButton != nullptr) {
        applyButton->setEnabled(enabled);
    }
}

void ConfigurationDialog::setRestoreDefaultsButtonEnabled(bool enabled)
{
    QPushButton *restoreDefaultsButton = buttonBox()->button(QDialogButtonBox::RestoreDefaults);
    if (restoreDefaultsButton != nullptr) {
        restoreDefaultsButton->setEnabled(enabled);
    }
}

void Konsole::ConfigurationDialog::showEvent(QShowEvent *event)
{
    if (!_shown) {
        _manager->updateWidgets();
        _groupManager->updateWidgets();

        bool hasChanged = _manager->hasChanged() || _groupManager->hasChanged();
        setApplyButtonEnabled(hasChanged);

        bool isDefault = _manager->isDefault() || _groupManager->isDefault();
        setRestoreDefaultsButtonEnabled(!isDefault);

        _shown = true;
    }
    KPageDialog::showEvent(event);
}
