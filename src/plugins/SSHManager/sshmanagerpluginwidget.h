/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSSHMANAGERPLUGINWIDGET_H
#define SSSHMANAGERPLUGINWIDGET_H

#include <QWidget>
#include <memory>

#include "sshconfigurationdata.h"

namespace Ui
{
class SSHTreeWidget;
}

namespace Konsole
{
class SessionController;
}
class SSHManagerModel;

class SSHManagerTreeWidget : public QWidget
{
    Q_OBJECT
public:
    SSHManagerTreeWidget(QWidget *parent = nullptr);
    ~SSHManagerTreeWidget() override;

    // shows the panel for add a new ssh info.
    Q_SLOT void showInfoPane();

    // hides the panel.
    Q_SLOT void hideInfoPane();

    // saves the currently new ssh info
    Q_SLOT void addSshInfo();

    // clears the panel
    Q_SLOT void clearSshInfo();

    // save the currently edited ssh info
    Q_SLOT void saveEdit();

    // display the panel for editing
    Q_SLOT void editSshInfo();

    // starts importing from ~/.ssh/config
    Q_SLOT void requestImport();

    Q_SLOT void handleTreeClick(Qt::MouseButton btn, const QModelIndex idx);

    Q_SIGNAL void requestNewTab();

    void setEditComponentsEnabled(bool enabled);

    void setModel(SSHManagerModel *model);
    void triggerRemove();
    void setCurrentController(Konsole::SessionController *controller);
    void connectRequested(const QModelIndex &idx);
    void handleImportedData(bool isImported);

private:
    std::pair<bool, QString> checkFields() const;
    SSHConfigurationData info() const;

    struct Private;
    std::unique_ptr<Ui::SSHTreeWidget> ui;
    std::unique_ptr<Private> d;
};

#endif
