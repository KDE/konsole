/*
    SPDX-FileCopyrightText: 2019 Mariusz Glebocki <mglb@arccos-1.net>

    Based on KConfigDialog and KConfigDialogManager from KConfigWidgets

    SPDX-FileCopyrightText: 2003 Benjamin C Meyer (ben+kdelibs at meyerhome dot net)
    SPDX-FileCopyrightText: 2003 Waldo Bastian <bastian@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef CONFIGURATIONDIALOG_H
#define CONFIGURATIONDIALOG_H

// Qt
#include <QAbstractButton>
#include <QButtonGroup>
#include <QMap>
#include <QTimer>

// KDE
#include <KCoreConfigSkeleton>
#include <KPageDialog>

// Konsole
#include "konsoleprivate_export.h"

class QWidget;
class KConfigDialogManager;

namespace Konsole
{
class ConfigDialogButtonGroupManager;

// KConfigDialog-like class, as the original KConfigDialog wraps
// all pages in QScrollArea. KConfigDialog, when fixed, should
// be source compatible with this class, so simple class replace
// should suffice.
class KONSOLEPRIVATE_EXPORT ConfigurationDialog : public KPageDialog
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
