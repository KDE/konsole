/*
 *  This file is part of Konsole, a terminal emulator for KDE.
 *
 *  Copyright 2019  Tomaz Canabrava <tcanabrava@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301  USA.
 */

#ifndef TERMINAL_HEADER_BAR_H
#define TERMINAL_HEADER_BAR_H

#include <QWidget>
#include <QPoint>
#include "Session.h"

class QLabel;
class QToolButton;
class QBoxLayout;
class QSplitter;

namespace Konsole {
    class TerminalDisplay;
    class ViewProperties;

class TerminalHeaderBar : public QWidget {
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
    void paintEvent(QPaintEvent* paintEvent) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    void mouseDoubleClickEvent(QMouseEvent *ev) override;

Q_SIGNALS:
    void requestToggleExpansion();

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
    QToolButton *m_toggleExpandedMode;
    bool m_terminalIsFocused;
    QPoint m_startDrag;
};

} // namespace Konsole

#endif
