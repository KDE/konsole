// This file was part of the KDE libraries
// SPDX-FileCopyrightText: 2022 Tao Guo <guotao945@gmail.com>
// SPDX-License-Identifier: GPL-2.0-or-later

#include "quickcommandswidget.h"
#include "filtermodel.h"
#include "konsoledebug.h"
#include "terminalDisplay/TerminalDisplay.h"

#include <QMenu>

#include "ui_qcwidget.h"
#include <KMessageBox>

struct QuickCommandsWidget::Private {
    QuickCommandsModel *model = nullptr;
    FilterModel *filterModel = nullptr;
    Konsole::SessionController *controller = nullptr;
};

QuickCommandsWidget::QuickCommandsWidget(QWidget *parent)
    : QWidget(parent)
    , ui(std::make_unique<Ui::QuickCommandsWidget>())
    , priv(std::make_unique<Private>())
{
    ui->setupUi(this);

    priv->filterModel = new FilterModel(this);
    connect(ui->btnAdd, &QPushButton::clicked, this, &QuickCommandsWidget::addMode);
    connect(ui->btnSave, &QPushButton::clicked, this, &QuickCommandsWidget::saveCommand);
    connect(ui->btnUpdate, &QPushButton::clicked, this, &QuickCommandsWidget::updateCommand);
    connect(ui->btnCancel, &QPushButton::clicked, this, &QuickCommandsWidget::viewMode);
    connect(ui->btnRun, &QPushButton::clicked, this, &QuickCommandsWidget::runCommand);

    connect(ui->invertFilter, &QPushButton::clicked, priv->filterModel, &FilterModel::setInvertFilter);

    connect(ui->filterLine, &QLineEdit::textChanged, this, [this] {
        priv->filterModel->setFilterRegularExpression(ui->filterLine->text());
        priv->filterModel->invalidate();
    });

    ui->commandsTreeView->setModel(priv->filterModel);
    ui->commandsTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->commandsTreeView, &QTreeView::doubleClicked, this, &QuickCommandsWidget::invokeCommand);
    connect(ui->commandsTreeView, &QTreeView::clicked, this, &QuickCommandsWidget::indexSelected);

    connect(ui->commandsTreeView, &QTreeView::customContextMenuRequested, [this](const QPoint &pos) {
        QModelIndex idx = ui->commandsTreeView->indexAt(pos);
        if (!idx.isValid())
            return;
        auto sourceIdx = priv->filterModel->mapToSource(idx);
        const bool isParent = sourceIdx.parent() == priv->model->invisibleRootItem()->index();
        QMenu *menu = new QMenu(this);

        if (!isParent) {
            auto actionEdit = new QAction(QStringLiteral("Edit"), ui->commandsTreeView);
            menu->addAction(actionEdit);
            connect(actionEdit, &QAction::triggered, this, &QuickCommandsWidget::editMode);
        } else {
            auto actionRename = new QAction(QStringLiteral("Rename"), ui->commandsTreeView);
            menu->addAction(actionRename);
            connect(actionRename, &QAction::triggered, this, &QuickCommandsWidget::triggerRename);
        }
        auto actionDelete = new QAction(QStringLiteral("Delete"), ui->commandsTreeView);
        menu->addAction(actionDelete);
        connect(actionDelete, &QAction::triggered, this, &QuickCommandsWidget::triggerDelete);
        menu->popup(ui->commandsTreeView->viewport()->mapToGlobal(pos));
    });
    viewMode();
}

QuickCommandsWidget::~QuickCommandsWidget() = default;

void QuickCommandsWidget::prepareEdit()
{
    QString groupName = ui->group->currentText();

    ui->group->clear();
    ui->group->addItems(priv->model->groups());
    ui->group->setCurrentText(groupName);
    ui->commandsTreeView->setDisabled(true);

    ui->commandsWidget->show();
}
void QuickCommandsWidget::viewMode()
{
    ui->commandsTreeView->setDisabled(false);
    ui->commandsWidget->hide();
    ui->btnAdd->show();
    ui->btnSave->hide();
    ui->btnUpdate->hide();
    ui->btnCancel->hide();
}

void QuickCommandsWidget::addMode()
{
    ui->btnAdd->hide();
    ui->btnSave->show();
    ui->btnUpdate->hide();
    ui->btnCancel->show();
    prepareEdit();
}

void QuickCommandsWidget::indexSelected(const QModelIndex &idx)
{
    const auto sourceIdx = priv->filterModel->mapToSource(ui->commandsTreeView->currentIndex());
    const auto item = priv->model->itemFromIndex(sourceIdx);
    const auto data = item->data(QuickCommandsModel::QuickCommandRole).value<QuickCommandData>();
    ui->name->setText(data.name);
    ui->tooltip->setText(data.tooltip);
    ui->command->setPlainText(data.command);
    ui->group->setCurrentText(item->parent()->text());
}

void QuickCommandsWidget::editMode()
{
    ui->btnAdd->hide();
    ui->btnSave->hide();
    ui->btnUpdate->show();
    ui->btnCancel->show();
    prepareEdit();
}

void QuickCommandsWidget::saveCommand()
{
    if (!valid())
        return;
    if (priv->model->addChildItem(data(), ui->group->currentText()))
        viewMode();
    else
        KMessageBox::messageBox(this, KMessageBox::DialogType::Error, i18n("A duplicate item exists"));
}

void QuickCommandsWidget::updateCommand()
{
    const auto sourceIdx = priv->filterModel->mapToSource(ui->commandsTreeView->currentIndex());
    if (!valid())
        return;
    if (priv->model->editChildItem(data(), sourceIdx, ui->group->currentText()))
        viewMode();
    else
        KMessageBox::messageBox(this, KMessageBox::DialogType::Error, i18n("A duplicate item exists"));
}

void QuickCommandsWidget::invokeCommand(const QModelIndex &idx)
{
    if (!priv->controller) {
        return;
    }
    const auto sourceIdx = priv->filterModel->mapToSource(idx);
    if (sourceIdx.parent() == priv->model->invisibleRootItem()->index()) {
        return;
    }
    const auto item = priv->model->itemFromIndex(sourceIdx);
    const auto data = item->data(QuickCommandsModel::QuickCommandRole).value<QuickCommandData>();
    priv->controller->session()->sendTextToTerminal(data.command, QLatin1Char('\r'));

    if (priv->controller->session()->views().count()) {
        priv->controller->session()->views().at(0)->setFocus();
    }
}

void QuickCommandsWidget::runCommand()
{
    const QString command = ui->command->toPlainText();
    priv->controller->session()->sendTextToTerminal(command, QLatin1Char('\r'));
    if (priv->controller->session()->views().count()) {
        priv->controller->session()->views().at(0)->setFocus();
    }
}

void QuickCommandsWidget::triggerRename()
{
    ui->commandsTreeView->edit(ui->commandsTreeView->currentIndex());
}

void QuickCommandsWidget::triggerDelete()
{
    const auto idx = ui->commandsTreeView->currentIndex();
    const QString text = idx.data(Qt::DisplayRole).toString();
    const QString dialogMessage = ui->commandsTreeView->model()->rowCount(idx)
        ? i18n("You are about to remove the group %1,\n with multiple configurations, are you sure?", text)
        : i18n("You are about to remove %1, are you sure?", text);

    KMessageBox::ButtonCode result = KMessageBox::messageBox(this,
                                                             KMessageBox::DialogType::WarningYesNo,
                                                             dialogMessage,
                                                             i18n("Remove Quick Commands Configurations"),
                                                             KStandardGuiItem::yes(),
                                                             KStandardGuiItem::no(),
                                                             KStandardGuiItem::cancel());
    if (result != KMessageBox::ButtonCode::Yes)
        return;

    const auto sourceIdx = priv->filterModel->mapToSource(idx);
    priv->model->removeRow(sourceIdx.row(), sourceIdx.parent());
}

QuickCommandData QuickCommandsWidget::data() const
{
    QuickCommandData data;
    data.name = ui->name->text().trimmed();
    data.tooltip = ui->tooltip->text();
    data.command = ui->command->toPlainText();
    return data;
}

void QuickCommandsWidget::setModel(QuickCommandsModel *model)
{
    priv->model = model;
    priv->filterModel->setSourceModel(model);
}
void QuickCommandsWidget::setCurrentController(Konsole::SessionController *controller)
{
    priv->controller = controller;
}

bool QuickCommandsWidget::valid()
{
    if (ui->name->text().isEmpty() || ui->name->text().trimmed().isEmpty()) {
        KMessageBox::messageBox(this, KMessageBox::DialogType::Error, i18n("Title can not be empty or blank"));
        return false;
    }
    if (ui->command->toPlainText().isEmpty()) {
        KMessageBox::messageBox(this, KMessageBox::DialogType::Error, i18n("Command can not be empty"));
        return false;
    }
    return true;
}
