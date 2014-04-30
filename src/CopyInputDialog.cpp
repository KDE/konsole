/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

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

// Own
#include "CopyInputDialog.h"

// Qt
#include <QSortFilterProxyModel>

// Konsole
#include "ui_CopyInputDialog.h"

#include <KLocalizedString>

using namespace Konsole;

CopyInputDialog::CopyInputDialog(QWidget* parent)
    : KDialog(parent)
{
    setCaption(i18n("Copy Input"));
    setButtons(KDialog::Ok | KDialog::Cancel);

    setWindowModality(Qt::WindowModal);

    _ui = new Ui::CopyInputDialog();
    _ui->setupUi(mainWidget());

    connect(_ui->selectAllButton, &QPushButton::clicked, this, &Konsole::CopyInputDialog::selectAll);
    connect(_ui->deselectAllButton, &QPushButton::clicked, this, &Konsole::CopyInputDialog::deselectAll);

    _ui->filterEdit->setClearButtonEnabled(true);
    _ui->filterEdit->setFocus();

    _model = new CheckableSessionModel(parent);
    _model->setCheckColumn(1);
    _model->setSessions(SessionManager::instance()->sessions());

    QSortFilterProxyModel* filterProxyModel = new QSortFilterProxyModel(this);
    filterProxyModel->setDynamicSortFilter(true);
    filterProxyModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    filterProxyModel->setSourceModel(_model);
    filterProxyModel->setFilterKeyColumn(-1);

    connect(_ui->filterEdit, &QLineEdit::textChanged,
            filterProxyModel, &QSortFilterProxyModel::setFilterFixedString);

    _ui->sessionList->setModel(filterProxyModel);
    _ui->sessionList->setColumnHidden(0, true); // Hide number column
    _ui->sessionList->header()->hide();
}

CopyInputDialog::~CopyInputDialog()
{
    delete _ui;
}

void CopyInputDialog::setChosenSessions(const QSet<Session*>& sessions)
{
    QSet<Session*> checked = sessions;
    if (_masterSession)
        checked.insert(_masterSession);

    _model->setCheckedSessions(checked);
}
QSet<Session*> CopyInputDialog::chosenSessions() const
{
    return _model->checkedSessions();
}
void CopyInputDialog::setMasterSession(Session* session)
{
    if (_masterSession)
        _model->setCheckable(_masterSession, true);

    _model->setCheckable(session, false);
    QSet<Session*> checked = _model->checkedSessions();
    checked.insert(session);
    _model->setCheckedSessions(checked);

    _masterSession = session;
}
void CopyInputDialog::setSelectionChecked(bool checked)
{
    QAbstractItemModel* model = _ui->sessionList->model();
    int rows = model->rowCount();

    QModelIndexList selected = _ui->sessionList->selectionModel()->selectedIndexes();

    if (selected.count() > 1) {
        foreach(const QModelIndex & index, selected) {
            setRowChecked(index.row(), checked);
        }
    } else {
        for (int i = 0; i < rows; i++)
            setRowChecked(i, checked);
    }
}
void CopyInputDialog::setRowChecked(int row, bool checked)
{
    QAbstractItemModel* model = _ui->sessionList->model();
    QModelIndex index = model->index(row, _model->checkColumn());
    if (checked)
        model->setData(index, static_cast<int>(Qt::Checked), Qt::CheckStateRole);
    else
        model->setData(index, static_cast<int>(Qt::Unchecked), Qt::CheckStateRole);
}
CheckableSessionModel::CheckableSessionModel(QObject* parent)
    : SessionListModel(parent)
    , _checkColumn(0)
{
}
void CheckableSessionModel::setCheckColumn(int column)
{
    _checkColumn = column;
    reset();
}
Qt::ItemFlags CheckableSessionModel::flags(const QModelIndex& index) const
{
    Session* session = static_cast<Session*>(index.internalPointer());

    if (_fixedSessions.contains(session))
        return SessionListModel::flags(index) & ~Qt::ItemIsEnabled;
    else
        return SessionListModel::flags(index) | Qt::ItemIsUserCheckable;
}
QVariant CheckableSessionModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::CheckStateRole && index.column() == _checkColumn) {
        Session* session = static_cast<Session*>(index.internalPointer());

        if (_checkedSessions.contains(session))
            return QVariant::fromValue(static_cast<int>(Qt::Checked));
        else
            return QVariant::fromValue(static_cast<int>(Qt::Unchecked));
    } else {
        return SessionListModel::data(index, role);
    }
}
bool CheckableSessionModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (role == Qt::CheckStateRole && index.column() == _checkColumn) {
        Session* session = static_cast<Session*>(index.internalPointer());

        if (_fixedSessions.contains(session))
            return false;

        if (value.value<int>() == Qt::Checked)
            _checkedSessions.insert(session);
        else
            _checkedSessions.remove(session);

        emit dataChanged(index, index);
        return true;
    } else {
        return SessionListModel::setData(index, value, role);
    }
}
void CheckableSessionModel::setCheckedSessions(const QSet<Session*> sessions)
{
    _checkedSessions = sessions;
    reset();
}
QSet<Session*> CheckableSessionModel::checkedSessions() const
{
    return _checkedSessions;
}
void CheckableSessionModel::setCheckable(Session* session, bool checkable)
{
    if (!checkable)
        _fixedSessions.insert(session);
    else
        _fixedSessions.remove(session);

    reset();
}
void CheckableSessionModel::sessionRemoved(Session* session)
{
    _checkedSessions.remove(session);
    _fixedSessions.remove(session);
}

