/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sshquickaccesswidget.h"

#include "sshmanagerfiltermodel.h"

#include <session/SessionController.h>
#include <terminalDisplay/TerminalDisplay.h>

#include <QAbstractItemModel>
#include <QBoxLayout>
#include <QDebug>
#include <QFocusEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QShowEvent>
#include <QTreeView>

struct SSHQuickAccessWidget::Private {
    QAbstractItemModel *model = nullptr;
    SSHManagerFilterModel *filterModel = nullptr;
    Konsole::SessionController *controller = nullptr;
    QTreeView *view = nullptr;
    QLineEdit *filterLine = nullptr;
};

SSHQuickAccessWidget::SSHQuickAccessWidget(QAbstractItemModel *model, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<SSHQuickAccessWidget::Private>())
{
    d->filterModel = new SSHManagerFilterModel(this);
    d->filterModel->setSourceModel(model);

    d->filterLine = new QLineEdit(this);
    d->filterLine->setPlaceholderText(tr("Filter"));

    auto *view = new QTreeView(this);
    view->setHeaderHidden(true);
    view->setModel(d->filterModel);
    d->view = view;

    auto *layout = new QBoxLayout(QBoxLayout::Direction::TopToBottom);
    layout->addWidget(d->filterLine);
    layout->addWidget(view);
    setLayout(layout);

    layout->setSpacing(0);
    d->model = model;

    connect(d->filterLine, &QLineEdit::textChanged, this, [this] {
        d->filterModel->setFilterRegularExpression(d->filterLine->text());
    });

    d->filterLine->installEventFilter(this);
}

SSHQuickAccessWidget::~SSHQuickAccessWidget() = default;

void SSHQuickAccessWidget::setSessionController(Konsole::SessionController *controller)
{
    d->controller = controller;
}

void SSHQuickAccessWidget::focusInEvent(QFocusEvent *ev)
{
    d->view->setFocus();
}

void SSHQuickAccessWidget::mousePressEvent(QMouseEvent *event)
{
}

void SSHQuickAccessWidget::keyPressEvent(QKeyEvent *ev)
{
    if (ev->key() == Qt::Key_Escape) {
        hide();
        d->controller->view()->setFocus();
    }
}

void SSHQuickAccessWidget::showEvent(QShowEvent *ev)
{
    if (!parentWidget()) {
        return;
    }

    auto rect = parentWidget()->geometry();
    int eigth = rect.width() / 8;
    rect.adjust(eigth, eigth, -eigth, -eigth);
    setGeometry(eigth, rect.topLeft().y(), rect.width(), rect.height());

    for (int i = 0; i < d->view->model()->rowCount(); i++) {
        QModelIndex idx = d->view->model()->index(i, 0);
        d->view->expand(idx);
    }
}

void SSHQuickAccessWidget::selectNext()
{
    constexpr QItemSelectionModel::SelectionFlag flag = QItemSelectionModel::SelectionFlag::ClearAndSelect;

    auto *slModel = d->view->selectionModel();
    auto selection = slModel->selectedIndexes();

    if (selection.empty()) {
        QModelIndex firstFolder = d->filterModel->index(0, 0);
        slModel->select(firstFolder, flag);
        return;
    }

    const QModelIndex curr = selection.first();
    const QModelIndex parent = curr.parent();
    const int currCount = d->filterModel->rowCount(curr);
    const int parentCount = d->filterModel->rowCount(parent);

    // we are currently on a folder, select the children.
    if (currCount != 0) {
        QModelIndex next = d->filterModel->index(0, 0, curr);
        slModel->select(next, flag);
        return;
    }

    // test to see if this is not the last possible child.
    if (curr.row() != parentCount - 1) {
        QModelIndex next = d->filterModel->index(curr.row() + 1, 0, curr.parent());
        slModel->select(next, flag);
        return;
    }

    // Last possible child. we need to go to the next parent, *or* wrap.
    if (parent.row() != d->filterModel->rowCount() - 1) {
        QModelIndex nextFolder = d->filterModel->index(parent.row() + 1, 0);
        slModel->select(nextFolder, flag);
        return;
    }

    // Wrap
    QModelIndex next = d->filterModel->index(0, 0);
    slModel->select(next, flag);
}

void SSHQuickAccessWidget::selectPrevious()
{
    constexpr QItemSelectionModel::SelectionFlag flag = QItemSelectionModel::SelectionFlag::ClearAndSelect;

    auto *slModel = d->view->selectionModel();
    auto selection = slModel->selectedIndexes();

    auto lambdaLastFolder = [this, slModel] {
        const QModelIndex lastFolder = d->filterModel->index(d->filterModel->rowCount() - 1, 0);
        const QModelIndex lastEntry = d->filterModel->index(d->filterModel->rowCount(lastFolder) - 1, 0, lastFolder);

        qDebug() << lastFolder << lastEntry;
        if (lastEntry.isValid()) {
            slModel->select(lastEntry, flag);
        } else {
            slModel->select(lastFolder, flag);
        }
        return;
    };

    if (selection.empty()) {
        lambdaLastFolder();
        return;
    }

    const QModelIndex curr = selection.first();
    const QModelIndex parent = curr.parent();
    const int currCount = d->filterModel->rowCount(curr);
    const int parentCount = d->filterModel->rowCount(parent);

    // we are currently on a folder, select the last element of the previous folder, or the previous folder
    // if it's empty.
    if (currCount != 0) {
        if (curr.row() == 0) {
            lambdaLastFolder();
            return;
        }

        QModelIndex prevFolder = d->filterModel->index(curr.row() - 1, 0);
        QModelIndex lastEntry = d->filterModel->index(d->filterModel->rowCount(prevFolder) - 1, 0, prevFolder);

        if (lastEntry.isValid()) {
            slModel->select(lastEntry, flag);
        } else {
            slModel->select(prevFolder, flag);
        }
        return;
    }

    // test to see if this is not the last possible child.
    if (curr.row() != 0) {
        QModelIndex next = d->filterModel->index(curr.row() - 1, 0, curr.parent());
        slModel->select(next, flag);
        return;
    }

    // Last possible child. we need to go to the next parent, *or* wrap.
    slModel->select(parent, flag);
}

bool SSHQuickAccessWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d->filterLine) {
        if (event->type() == QEvent::KeyPress) {
            qDebug() << "Event" << event->type();
            auto keyEvent = dynamic_cast<QKeyEvent *>(event);
            switch (keyEvent->key()) {
            case Qt::Key_Up:
                selectPrevious();
                break;
            case Qt::Key_Down:
                selectNext();
                break;
            default:
                break;
            }
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    if (watched == parentWidget()) {
        if (event->type() == QEvent::MouseButtonPress) {
            hide();
            d->controller->view()->setFocus();
            removeEventFilter(watched);
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    return QWidget::eventFilter(watched, event);
}
