/*
    SPDX-FileCopyrightText: 2010 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef RENAMETABWIDGET_H
#define RENAMETABWIDGET_H

// Qt
#include <QWidget>

class QColor;

namespace Ui
{
class RenameTabWidget;
}

namespace Konsole
{
class RenameTabWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RenameTabWidget(QWidget *parent = nullptr);
    ~RenameTabWidget() override;

    QString tabTitleText() const;
    QString remoteTabTitleText() const;
    QColor color() const;
    void setTabTitleText(const QString &);
    void setRemoteTabTitleText(const QString &);
    void setColor(const QColor &);

    void focusTabTitleText();
    void focusRemoteTabTitleText();

Q_SIGNALS:
    void tabTitleFormatChanged(const QString &);
    void remoteTabTitleFormatChanged(const QString &);
    void tabColorChanged(const QColor &);

public Q_SLOTS:
    void insertTabTitleText(const QString &text);
    void insertRemoteTabTitleText(const QString &text);

private:
    Ui::RenameTabWidget *_ui;
    QColor _colorNone;
};
}

#endif
