/*
    SPDX-FileCopyrightText: 2024 Troy Hoover <troytjh98@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Own
#include "SearchTabsModel.h"

// Qt
#include <QEvent>
#include <QFrame>
#include <QLineEdit>
#include <QTreeView>

// Konsole
#include "ViewManager.h"
#include "widgets/ViewContainer.h"

class SearchTabsFilterProxyModel;

namespace Konsole
{

class KONSOLEPRIVATE_EXPORT SearchTabs : public QFrame
{
public:
    SearchTabs(ViewManager *viewManager);

    /**
     * update state
     * will fill model with current open tabs
     */
    void updateState();
    void updateViewGeometry();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void reselectFirst();

    /**
     * Return pressed, activate the selected tab
     * and go back to background
     */
    void slotReturnPressed();

private:
    ViewManager *m_viewManager;

    QLineEdit *m_inputLine;
    QTreeView *m_listView;

    /**
     * tab model
     */
    SearchTabsModel *m_model = nullptr;

    /**
     * fuzzy filter model
     */
    SearchTabsFilterProxyModel *m_proxyModel = nullptr;
};

}
