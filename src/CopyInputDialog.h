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

#ifndef COPYINPUTDIALOG
#define COPYINPUTDIALOG

// Qt
#include <QPointer>

// KDE
#include <KDialog>

// Konsole
#include "SessionManager.h"
#include "Session.h"

namespace Ui
{
    class CopyInputDialog;
}

namespace Konsole
{
class CheckableSessionModel;

class CopyInputDialog : public KDialog
{
Q_OBJECT

public:
    CopyInputDialog(QWidget* parent = 0);

    void setMasterSession(Session* master);
    Session* masterSession() const;

    void setChosenSessions(const QSet<Session*>& sessions);
    QSet<Session*> chosenSessions() const;

private slots:
    void selectAll() { setSelectionChecked(true); };
    void deselectAll() { setSelectionChecked(false); };

private:
    void setSelectionChecked(bool checked);
    void setRowChecked(int row, bool checked);

    Ui::CopyInputDialog* _ui;
    CheckableSessionModel* _model;
    QPointer<Session> _masterSession;
};

class CheckableSessionModel : public SessionListModel
{
Q_OBJECT 

public:
    CheckableSessionModel(QObject* parent); 

    void setCheckColumn(int column);
    int checkColumn() const;

    void setCheckable(Session* session, bool checkable);
    
    void setCheckedSessions(const QSet<Session*> sessions);
    QSet<Session*> checkedSessions() const;

    virtual Qt::ItemFlags flags(const QModelIndex& index) const;
    virtual QVariant data(const QModelIndex& index, int role) const;
    virtual bool setData(const QModelIndex& index, const QVariant& value, int role);

protected:
    virtual void sessionRemoved(Session*);

private:
    QSet<Session*> _checkedSessions;
    QSet<Session*> _fixedSessions;
    int _checkColumn;
};
inline int CheckableSessionModel::checkColumn() const
{ return _checkColumn; }

}

#endif // COPYINPUTDIALOG

