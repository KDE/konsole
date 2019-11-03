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

#include "TerminalHeaderBar.h"

#include "TerminalDisplay.h"
#include "SessionController.h"
#include "ViewProperties.h"

#include <KLocalizedString>
#include <QBoxLayout>
#include <QToolButton>
#include <QLabel>
#include <QApplication>
#include <QPaintEvent>
#include <QTabBar>
#include <QPainter>
#include <QSplitter>
#include <QStyleOptionTabBarBase>
#include <QStylePainter>
#include <QDrag>
#include <QMimeData>

namespace Konsole {

TerminalHeaderBar::TerminalHeaderBar(QWidget *parent)
    : QWidget(parent)
{
    m_closeBtn = new QToolButton(this);
    m_closeBtn->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    m_closeBtn->setToolTip(i18nc("@info:tooltip", "Close terminal"));
    m_closeBtn->setText(i18nc("@info:tooltip", "Close terminal"));
    m_closeBtn->setObjectName(QStringLiteral("close-terminal-button"));
    m_closeBtn->setAutoRaise(true);

    m_toggleExpandedMode = new QToolButton(this);
    m_toggleExpandedMode->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen"))); // fake 'expand' icon. VDG input?
    m_toggleExpandedMode->setAutoRaise(true);
    m_toggleExpandedMode->setCheckable(true);
    m_toggleExpandedMode->setToolTip(i18nc("@info:tooltip", "Maximize terminal"));

    m_terminalTitle = new QLabel(this);
    m_terminalTitle->setFont(QApplication::font());

    m_terminalIcon = new QLabel(this);
    m_terminalActivity = new QLabel(this);

    m_boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_boxLayout->setSpacing(0);
    m_boxLayout->setContentsMargins(0, 0, 0, 0);

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
    connect(m_toggleExpandedMode, &QToolButton::clicked,
        this, &TerminalHeaderBar::requestToggleExpansion);

}

// Hack untill I can detangle the creation of the TerminalViews
void TerminalHeaderBar::finishHeaderSetup(ViewProperties *properties)
{
    auto controller = dynamic_cast<SessionController*>(properties);
    connect(properties, &Konsole::ViewProperties::titleChanged, this, [this, properties]{
        m_terminalTitle->setText(properties->title());
    });

    connect(properties, &Konsole::ViewProperties::iconChanged, this, [this, properties] {
        m_terminalIcon->setPixmap(properties->icon().pixmap(QSize(22,22)));
    });

    connect(properties, &Konsole::ViewProperties::activity, this, [this]{
        m_terminalActivity->setPixmap(QPixmap());
    });

    connect(m_closeBtn, &QToolButton::clicked, controller, &SessionController::closeSession);
}

void TerminalHeaderBar::paintEvent(QPaintEvent *paintEvent)
{
    /* Try to get the widget that's 10px above this one.
     * If the widget is something else than a TerminalWidget, a TabBar or a QSplitter,
     * draw a 1px line to separate it from the others.
     */

    const auto globalPos = parentWidget()->mapToGlobal(pos());
    auto *widget = qApp->widgetAt(globalPos.x() + 10, globalPos.y() - 10);

    const bool isTabbar = qobject_cast<QTabBar*>(widget) != nullptr;
    const bool isTerminalWidget = qobject_cast<TerminalDisplay*>(widget) != nullptr;
    const bool isSplitter = (qobject_cast<QSplitter*>(widget) != nullptr) || (qobject_cast<QSplitterHandle*>(widget) != nullptr);
    if ((widget != nullptr) && !isTabbar && !isTerminalWidget && !isSplitter) {
        QStyleOptionTabBarBase optTabBase;
        QStylePainter p(this);
        optTabBase.init(this);
        optTabBase.shape = QTabBar::Shape::RoundedSouth;
        optTabBase.documentMode = false;
        p.drawPrimitive(QStyle::PE_FrameTabBarBase, optTabBase);
    }

    QWidget::paintEvent(paintEvent);
    if (!m_terminalIsFocused) {
        auto p = qApp->palette();
        auto shadowColor = p.color(QPalette::ColorRole::Shadow);
        shadowColor.setAlphaF( 0.2f * shadowColor.alphaF() ); // same as breeze.

        QPainter painter(this);
        painter.setPen(Qt::NoPen);
        painter.setBrush(shadowColor);
        painter.drawRect(rect());
    }
}

void TerminalHeaderBar::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_toggleExpandedMode->isChecked()) {
        return;
    }
    auto point = ev->pos() - m_startDrag;
    if (point.manhattanLength() > 10) {
        auto drag = new QDrag(parent());
        auto mimeData = new QMimeData();
        QByteArray payload;
        payload.setNum(qApp->applicationPid());
        mimeData->setData(QStringLiteral("konsole/terminal_display"), payload);
        drag->setMimeData(mimeData);
        drag->exec();
    }
}

void TerminalHeaderBar::mousePressEvent(QMouseEvent* ev)
{
    m_startDrag = ev->pos();
}

void TerminalHeaderBar::mouseReleaseEvent(QMouseEvent* ev)
{
    Q_UNUSED(ev);
}

void TerminalHeaderBar::terminalFocusIn()
{
    m_terminalIsFocused = true;
    update();
}

void TerminalHeaderBar::terminalFocusOut()
{
    m_terminalIsFocused = false;
    update();
}

}
