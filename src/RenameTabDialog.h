/*
    SPDX-FileCopyrightText: 2010 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RENAMETABDIALOG_H
#define RENAMETABDIALOG_H

// KDE
#include <QDialog>

namespace Ui
{
class RenameTabDialog;
}

namespace Konsole
{
class RenameTabDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RenameTabDialog(QWidget *parent = nullptr);
    ~RenameTabDialog() override;

    QString tabTitleText() const;
    QString remoteTabTitleText() const;
    QColor color() const;
    void setTabTitleText(const QString &);
    void setRemoteTabTitleText(const QString &);
    void setColor(const QColor &);

    void focusTabTitleText();
    void focusRemoteTabTitleText();

private:
    Q_DISABLE_COPY(RenameTabDialog)

    Ui::RenameTabDialog *_ui;
};
}

#endif
