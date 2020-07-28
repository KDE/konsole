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

}

#endif // CONFIGURATIONDIALOG_H
