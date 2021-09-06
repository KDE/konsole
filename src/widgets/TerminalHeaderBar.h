/*
 *  SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef TERMINAL_HEADER_BAR_H
#define TERMINAL_HEADER_BAR_H

#include "session/Session.h"
#include <QPoint>
#include <QWidget>

class QLabel;
class QToolButton;
class QBoxLayout;
class QSplitter;

namespace Konsole
{
class TerminalDisplay;
class ViewProperties;

class TerminalHeaderBar : public QWidget
{
    Q_OBJECT
public:
    // TODO: Verify if the terminalDisplay is needed, or some other thing like SessionController.
    explicit TerminalHeaderBar(QWidget *parent = nullptr);
    void finishHeaderSetup(ViewProperties *properties);
    QSize minimumSizeHint() const override;
    void applyVisibilitySettings();
    QSplitter *getTopLevelSplitter();
    void setExpandedMode(bool expand);

public Q_SLOTS:
    void setFocusIndicatorState(bool focused);
    /** Shows/hide notification status icon */
    void updateNotification(ViewProperties *item, Konsole::Session::Notification notification, bool enabled);
    /** Shows/hide special state status icon (copy input or read-only) */
    void updateSpecialState(ViewProperties *item);

protected:
    void paintEvent(QPaintEvent *paintEvent) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseDoubleClickEvent(QMouseEvent *ev) override;

Q_SIGNALS:
    void requestToggleExpansion();
    void requestMoveToNewTab();

private:
    QBoxLayout *m_boxLayout;
    QLabel *m_terminalTitle;
    QLabel *m_terminalIcon;
    QLabel *m_statusIconReadOnly;
    QLabel *m_statusIconCopyInput;
    QLabel *m_statusIconSilence;
    QLabel *m_statusIconActivity;
    QLabel *m_statusIconBell;
    QToolButton *m_closeBtn;
    QToolButton *m_moveToNewTab;
    QToolButton *m_toggleExpandedMode;
    bool m_terminalIsFocused;
    QPoint m_startDrag;
};

} // namespace Konsole

#endif
