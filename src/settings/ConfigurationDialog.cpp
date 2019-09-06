/*
    Copyright 2019 by Mariusz Glebocki <mglb@arccos-1.net>

    Based on KConfigDialog from KConfigWidgets

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

// Own
#include "ConfigurationDialog.h"

// Qt
#include <QPushButton>
#include <QDialogButtonBox>

// KDE
#include <KLocalizedString>
#include <KConfigDialogManager>


using namespace Konsole;


const QString ConfigDialogButtonGroupManager::ManagedNamePrefix = QStringLiteral("kcfg_");


ConfigurationDialog::ConfigurationDialog(QWidget *parent, KCoreConfigSkeleton *config)
    : KPageDialog(parent)
{
    setWindowTitle(i18nc("@title:window", "Configure"));
    // Setting this after modifying buttonBox results in initial focus set to buttonBox.
    setFaceType(KPageDialog::List);

    buttonBox()->setStandardButtons(QDialogButtonBox::RestoreDefaults
                                  | QDialogButtonBox::Ok
                                  | QDialogButtonBox::Apply
                                  | QDialogButtonBox::Cancel);
    connect(buttonBox()->button(QDialogButtonBox::Apply), &QAbstractButton::clicked,
            this, &ConfigurationDialog::updateButtons);
    connect(buttonBox()->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked,
            this, &ConfigurationDialog::updateButtons);

    _manager = new KConfigDialogManager(this, config);
    connect(_manager, SIGNAL(settingsChanged()), this, SLOT(settingsChangedSlot()));
    connect(_manager, SIGNAL(widgetModified()),  this, SLOT(updateButtons()));

    connect(buttonBox()->button(QDialogButtonBox::Ok), &QAbstractButton::clicked,
            _manager, &KConfigDialogManager::updateSettings);
    connect(buttonBox()->button(QDialogButtonBox::Apply), &QAbstractButton::clicked,
            _manager, &KConfigDialogManager::updateSettings);
    connect(buttonBox()->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked,
            _manager, &KConfigDialogManager::updateWidgets);
    connect(buttonBox()->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked,
            _manager, &KConfigDialogManager::updateWidgetsDefault);

    _groupManager = new ConfigDialogButtonGroupManager(this, config);
    connect(_groupManager, SIGNAL(settingsChanged()), this, SLOT(settingsChangedSlot()));
    connect(_groupManager, SIGNAL(widgetModified()),  this, SLOT(updateButtons()));

    connect(buttonBox()->button(QDialogButtonBox::Ok), &QAbstractButton::clicked,
            _groupManager, &ConfigDialogButtonGroupManager::updateSettings);
    connect(buttonBox()->button(QDialogButtonBox::Apply), &QAbstractButton::clicked,
            _groupManager, &ConfigDialogButtonGroupManager::updateSettings);
    connect(buttonBox()->button(QDialogButtonBox::Cancel), &QAbstractButton::clicked,
            _groupManager, &ConfigDialogButtonGroupManager::updateWidgets);
    connect(buttonBox()->button(QDialogButtonBox::RestoreDefaults), &QAbstractButton::clicked,
            _groupManager, &ConfigDialogButtonGroupManager::updateWidgetsDefault);

    setApplyButtonEnabled(false);
}

void ConfigurationDialog::addPage(KPageWidgetItem *item, bool manage)
{
    Q_ASSERT(item);
    Q_ASSERT(item->widget());

    KPageDialog::addPage(item);

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

    emit widgetModified();
    onlyOnce = false;
}

void ConfigurationDialog::settingsChangedSlot()
{
    updateButtons();
    emit settingsChanged();
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
