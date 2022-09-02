/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef SSHQUICKACCESSWIDGET
#define SSHQUICKACCESSWIDGET

#include <QWidget>

#include <memory>

class QAbstractItemModel;

namespace Konsole
{
class SessionController;
};

/* A Widget invoked by shortcut to quickly fill something
 * on the terminal
 */
class SSHQuickAccessWidget : public QWidget
{
    Q_OBJECT
public:
    struct Private;
    SSHQuickAccessWidget(QAbstractItemModel *model, QWidget *parent = nullptr);
    ~SSHQuickAccessWidget();
    void setSessionController(Konsole::SessionController *controller);

    void selectPrevious();
    void selectNext();

    void showEvent(QShowEvent *ev) override;
    void focusInEvent(QFocusEvent *ev) override;
    void keyPressEvent(QKeyEvent *ev) override;
    void mousePressEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    std::unique_ptr<Private> d;
};

#endif
