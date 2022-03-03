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

    Q_SLOT void triggerEdit();

    Q_SLOT void triggerRename();

    Q_SLOT void triggerDelete();

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
