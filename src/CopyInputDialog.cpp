/*
    SPDX-FileCopyrightText: 2008 Robert Knight <robertknight@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "CopyInputDialog.h"

// Qt
#include <QSortFilterProxyModel>

// Konsole
#include "CheckableSessionModel.h"
#include "ui_CopyInputDialog.h"

#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Konsole;

CopyInputDialog::CopyInputDialog(QWidget *parent)
    : QDialog(parent)
    , _ui(nullptr)
    , _model(nullptr)
    , _masterSession(nullptr)
{
    setWindowTitle(i18n("Copy Input"));
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto mainWidget = new QWidget(this);
    auto mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &CopyInputDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CopyInputDialog::reject);
    mainLayout->addWidget(buttonBox);

    setWindowModality(Qt::WindowModal);

    _ui = new Ui::CopyInputDialog();
    _ui->setupUi(mainWidget);

    connect(_ui->selectAllButton, &QPushButton::clicked, this, &Konsole::CopyInputDialog::selectAll);
    connect(_ui->deselectAllButton, &QPushButton::clicked, this, &Konsole::CopyInputDialog::deselectAll);

    _ui->filterEdit->setClearButtonEnabled(true);
    _ui->filterEdit->setFocus();

    _model = new CheckableSessionModel(parent);
    _model->setCheckColumn(1);
    _model->setSessions(SessionManager::instance()->sessions());

    auto filterProxyModel = new QSortFilterProxyModel(this);
    filterProxyModel->setDynamicSortFilter(true);
    filterProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterProxyModel->setSourceModel(_model);
    filterProxyModel->setFilterKeyColumn(-1);

    connect(_ui->filterEdit, &QLineEdit::textChanged, filterProxyModel, &QSortFilterProxyModel::setFilterFixedString);

    _ui->sessionList->setModel(filterProxyModel);
    _ui->sessionList->setColumnHidden(0, true); // Hide number column
    _ui->sessionList->header()->hide();
}

CopyInputDialog::~CopyInputDialog()
{
    delete _ui;
}

void CopyInputDialog::setChosenSessions(const QSet<Session *> &sessions)
{
    QSet<Session *> checked = sessions;
    if (!_masterSession.isNull()) {
        checked.insert(_masterSession);
    }

    _model->setCheckedSessions(checked);
}

QSet<Session *> CopyInputDialog::chosenSessions() const
{
    return _model->checkedSessions();
}

void CopyInputDialog::setMasterSession(Session *session)
{
    if (!_masterSession.isNull()) {
        _model->setCheckable(_masterSession, true);
    }

    _model->setCheckable(session, false);
    QSet<Session *> checked = _model->checkedSessions();
    checked.insert(session);
    _model->setCheckedSessions(checked);

    _masterSession = session;
}

void CopyInputDialog::setSelectionChecked(bool checked)
{
    QAbstractItemModel *model = _ui->sessionList->model();
    int rows = model->rowCount();

    const QModelIndexList selected = _ui->sessionList->selectionModel()->selectedIndexes();

    if (selected.count() > 1) {
        for (const QModelIndex &index : selected) {
            setRowChecked(index.row(), checked);
        }
    } else {
        for (int i = 0; i < rows; i++) {
            setRowChecked(i, checked);
        }
    }
}

void CopyInputDialog::setRowChecked(int row, bool checked)
{
    QAbstractItemModel *model = _ui->sessionList->model();
    QModelIndex index = model->index(row, _model->checkColumn());
    model->setData(index, static_cast<int>(checked ? Qt::Checked : Qt::Unchecked), Qt::CheckStateRole);
}
