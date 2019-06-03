/*
 *  This file is part of Konsole, a terminal emulator for KDE.
 *
 *  Copyright 2012  Frederik Gladhorn <gladhorn@kde.org>
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

#include "TerminalHeaderBar.h"

#include "TerminalDisplay.h"
#include "SessionController.h"
#include "ViewProperties.h"

#include <KLocalizedString>
#include <QBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QLabel>
#include <QAction>
#include <KToolBarLabelAction>
#include <QToolButton>
#include <QDebug>
#include <QApplication>

namespace Konsole {

TerminalHeaderBar::TerminalHeaderBar(QWidget *parent)
    : QWidget(parent)
{
    m_closeBtn = new QToolButton(this);
    m_closeBtn->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    m_closeBtn->setToolTip(i18nc("@action:itooltip", "Close terminal"));
    m_closeBtn->setText(i18nc("@action:itooltip", "Close terminal"));
    m_closeBtn->setObjectName(QStringLiteral("close-terminal-button"));
    m_closeBtn->setAutoRaise(true);

    m_toggleExpandedMode = new QToolButton(this);
    m_toggleExpandedMode->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen"))); // fake 'expand' icon. VDG input?
    m_toggleExpandedMode->setAutoRaise(true);
    m_toggleExpandedMode->setCheckable(true);
    m_toggleExpandedMode->setToolTip(i18nc("@action:itooltip", "Maximize / Restore Terminal"));

    m_terminalTitle = new QLabel(this);
    m_terminalIcon = new QLabel(this);
    m_terminalActivity = new QLabel(this);

    m_boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_boxLayout->setSpacing(0);
    m_boxLayout->setMargin(0);

    // Layout Setup
    m_boxLayout->addStretch();
    m_boxLayout->addWidget(m_terminalIcon);
    m_boxLayout->addWidget(m_terminalTitle);
    m_boxLayout->addWidget(m_terminalActivity);
    m_boxLayout->addStretch();
    m_boxLayout->addWidget(m_toggleExpandedMode);
    m_boxLayout->addWidget(m_closeBtn);
    setLayout(m_boxLayout);

    setAutoFillBackground(true);
    terminalFocusOut();
}

// Hack untill I can detangle the creation of the TerminalViews
void TerminalHeaderBar::finishHeaderSetup(ViewProperties *properties)
{
    //TODO: Fix ViewProperties signals.
    connect(properties, &Konsole::ViewProperties::titleChanged, this,
    [this, properties]{
        m_terminalTitle->setText(properties->title());
    });

    connect(m_closeBtn, &QToolButton::clicked, this, [properties]{
        auto controller = qobject_cast<SessionController*>(properties);
        controller->closeSession();
    });

    connect(properties, &Konsole::ViewProperties::iconChanged, this, [this, properties] {
        m_terminalIcon->setPixmap(properties->icon().pixmap(QSize(22,22)));
    });

    connect(properties, &Konsole::ViewProperties::activity, this, [this]{
        m_terminalActivity->setPixmap(QPixmap());
    });

    connect(m_toggleExpandedMode, &QToolButton::clicked,
            this, &TerminalHeaderBar::requestToggleExpansion);
}

void TerminalHeaderBar::terminalFocusIn()
{
    setPalette(qApp->palette());
    m_terminalTitle->setEnabled(true);
}

void TerminalHeaderBar::terminalFocusOut()
{
    auto p = palette();
    p.setBrush(QPalette::ColorRole::Window, p.brush(QPalette::ColorRole::Window).color().dark());
    setPalette(p);
    m_terminalTitle->setEnabled(false);
}

}
