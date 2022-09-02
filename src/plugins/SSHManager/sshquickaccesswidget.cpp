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
};

SSHQuickAccessWidget::SSHQuickAccessWidget(QAbstractItemModel *model, QWidget *parent)
    : QWidget(parent)
    , d(std::make_unique<SSHQuickAccessWidget::Private>())
{
    d->filterModel = new SSHManagerFilterModel(this);
    d->filterModel->setSourceModel(model);

    auto *filter = new QLineEdit(this);
    filter->setPlaceholderText(tr("Filter"));

    auto *view = new QTreeView(this);
    view->setHeaderHidden(true);
    view->setModel(d->filterModel);
    d->view = view;

    auto *layout = new QBoxLayout(QBoxLayout::Direction::TopToBottom);
    layout->addWidget(filter);
    layout->addWidget(view);
    setLayout(layout);

    layout->setSpacing(0);
    d->model = model;

    connect(filter, &QLineEdit::textChanged, this, [this, filter] {
        d->filterModel->setFilterRegularExpression(filter->text());
    });
}

SSHQuickAccessWidget::~SSHQuickAccessWidget() = default;

void SSHQuickAccessWidget::setSessionController(Konsole::SessionController *controller)
{
    d->controller = controller;
}

void SSHQuickAccessWidget::focusInEvent(QFocusEvent *ev)
{
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

bool SSHQuickAccessWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        hide();
        d->controller->view()->setFocus();
        removeEventFilter(watched);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
