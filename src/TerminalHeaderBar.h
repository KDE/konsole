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

class QLabel;
class QToolButton;
class QBoxLayout;
namespace Konsole {
    class TerminalDisplay;
    class ViewProperties;

class TerminalHeaderBar : public QWidget {
    Q_OBJECT
public:
    // TODO: Verify if the terminalDisplay is needed, or some other thing like SessionController.
    explicit TerminalHeaderBar(QWidget *parent = nullptr);
    void finishHeaderSetup(ViewProperties *properties);

    void terminalFocusIn();
    void terminalFocusOut();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;

Q_SIGNALS:
    void requestToggleExpansion();

private:
    QBoxLayout *m_boxLayout;
    TerminalDisplay *m_terminalDisplay;
    QLabel *m_terminalTitle;
    QLabel *m_terminalIcon;
    QLabel *m_terminalActivity; // Bell icon.
    QToolButton *m_closeBtn;
    QToolButton *m_toggleExpandedMode;
    bool m_terminalIsFocused;
    QPoint m_startDrag;
};

} // namespace Konsole

#endif
