// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef QUICKCOMMANDSWIDGET_H
#define QUICKCOMMANDSWIDGET_H

#include <QWidget>
#include <memory>

#include "quickcommanddata.h"
#include "quickcommandsmodel.h"

#include "session/SessionController.h"

namespace Ui
{
class QuickCommandsWidget;
}

class QuickCommandsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit QuickCommandsWidget(QWidget *parent = nullptr);
    ~QuickCommandsWidget() override;

    Q_SLOT void viewMode();

    Q_SLOT void addMode();

    Q_SLOT void editMode();

    Q_SLOT void saveCommand();

    Q_SLOT void updateCommand();

    Q_SLOT void invokeCommand(const QModelIndex &idx);
    // Run a command from the currently selected Tree element

    Q_SLOT void runCommand();
    // Run the command from the Text Area

    Q_SLOT void indexSelected(const QModelIndex &idx);

    Q_SLOT void triggerRename();

    Q_SLOT void triggerDelete();

    Q_SLOT void createMenu(const QPoint &pos);

    Q_SLOT void runShellCheck();

    void setModel(QuickCommandsModel *model);
    void setCurrentController(Konsole::SessionController *controller);

private:
    QuickCommandData data() const;
    void prepareEdit();
    bool valid();
    struct Private;

    std::unique_ptr<Ui::QuickCommandsWidget> ui;
    std::unique_ptr<Private> priv;
};

#endif // QUICKCOMMANDSWIDGET_H
