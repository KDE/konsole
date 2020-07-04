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
#include "KonsoleSettings.h"
#include "ViewSplitter.h"

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
    m_boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_boxLayout->setSpacing(0);
    m_boxLayout->setContentsMargins(0, 0, 0, 0);

    // Session icon

    m_terminalIcon = new QLabel(this);
    m_terminalIcon->setAlignment(Qt::AlignCenter);
    m_terminalIcon->setFixedSize(20, 20);

    m_boxLayout->addWidget(m_terminalIcon);

    // Status icons

    QLabel ** statusIcons[] = {&m_statusIconReadOnly, &m_statusIconCopyInput, &m_statusIconSilence, &m_statusIconActivity, &m_statusIconBell};

    for (auto **statusIcon: statusIcons) {
        *statusIcon = new QLabel(this);
        (*statusIcon)->setAlignment(Qt::AlignCenter);
        (*statusIcon)->setFixedSize(20, 20);
        (*statusIcon)->setVisible(false);

        m_boxLayout->addWidget(*statusIcon);
    }

    m_statusIconReadOnly->setPixmap(QIcon::fromTheme(QStringLiteral("object-locked")).pixmap(QSize(16,16)));
    m_statusIconCopyInput->setPixmap(QIcon::fromTheme(QStringLiteral("irc-voice")).pixmap(QSize(16,16)));
    m_statusIconSilence->setPixmap(QIcon::fromTheme(QStringLiteral("system-suspend")).pixmap(QSize(16,16)));
    m_statusIconActivity->setPixmap(QIcon::fromTheme(QStringLiteral("dialog-information")).pixmap(QSize(16,16)));
    m_statusIconBell->setPixmap(QIcon::fromTheme(QStringLiteral("notifications")).pixmap(QSize(16,16)));

    // Title

    m_terminalTitle = new QLabel(this);
    m_terminalTitle->setFont(QApplication::font());

    m_boxLayout->addStretch();
    m_boxLayout->addWidget(m_terminalTitle);
    m_boxLayout->addStretch();

    // Expand button

    m_toggleExpandedMode = new QToolButton(this);
    m_toggleExpandedMode->setIcon(QIcon::fromTheme(QStringLiteral("view-fullscreen"))); // fake 'expand' icon. VDG input?
    m_toggleExpandedMode->setAutoRaise(true);
    m_toggleExpandedMode->setCheckable(true);
    m_toggleExpandedMode->setToolTip(i18nc("@info:tooltip", "Maximize terminal"));

    connect(m_toggleExpandedMode, &QToolButton::clicked,
        this, &TerminalHeaderBar::requestToggleExpansion);

    m_boxLayout->addWidget(m_toggleExpandedMode);

    // Close button

    m_closeBtn = new QToolButton(this);
    m_closeBtn->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    m_closeBtn->setToolTip(i18nc("@info:tooltip", "Close terminal"));
    m_closeBtn->setText(i18nc("@info:tooltip", "Close terminal"));
    m_closeBtn->setObjectName(QStringLiteral("close-terminal-button"));
    m_closeBtn->setAutoRaise(true);

    m_boxLayout->addWidget(m_closeBtn);

    // The widget itself

    setLayout(m_boxLayout);
    setAutoFillBackground(true);
    setFocusIndicatorState(false);
}

void TerminalHeaderBar::mouseDoubleClickEvent(QMouseEvent *ev)
{
    if (ev->button() != Qt::LeftButton) {
        return;
    }
    m_toggleExpandedMode->click();
}

// Hack untill I can detangle the creation of the TerminalViews
void TerminalHeaderBar::finishHeaderSetup(ViewProperties *properties)
{
    auto controller = dynamic_cast<SessionController*>(properties);
    connect(properties, &Konsole::ViewProperties::titleChanged, this, [this, properties]{
        m_terminalTitle->setText(properties->title());
    });
    m_terminalTitle->setText(properties->title());

    connect(properties, &Konsole::ViewProperties::iconChanged, this, [this, properties] {
        m_terminalIcon->setPixmap(properties->icon().pixmap(QSize(22,22)));
    });
    m_terminalIcon->setPixmap(properties->icon().pixmap(QSize(22,22)));

    connect(properties, &Konsole::ViewProperties::notificationChanged, this,
            &Konsole::TerminalHeaderBar::updateNotification);

    connect(properties, &Konsole::ViewProperties::readOnlyChanged, this,
            &Konsole::TerminalHeaderBar::updateSpecialState);

    connect(properties, &Konsole::ViewProperties::copyInputChanged, this,
            &Konsole::TerminalHeaderBar::updateSpecialState);

    connect(m_closeBtn, &QToolButton::clicked, controller, &SessionController::closeSession);
}

void TerminalHeaderBar::setFocusIndicatorState(bool focused)
{
    m_terminalIsFocused = focused;
    update();
}

void TerminalHeaderBar::updateNotification(ViewProperties *item, Session::Notification notification, bool enabled)
{
    Q_UNUSED(item)

    switch(notification) {
    case Session::Notification::Silence:
        m_statusIconSilence->setVisible(enabled);
        break;
    case Session::Notification::Activity:
        m_statusIconActivity->setVisible(enabled);
        break;
    case Session::Notification::Bell:
        m_statusIconBell->setVisible(enabled);
        break;
    default:
        break;
    }
}

void TerminalHeaderBar::updateSpecialState(ViewProperties *item)
{
    auto controller = dynamic_cast<SessionController*>(item);

    if (controller != nullptr) {
        m_statusIconReadOnly->setVisible(controller->isReadOnly());
        m_statusIconCopyInput->setVisible(controller->isCopyInputActive());
    }
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
        shadowColor.setAlphaF( qreal(0.2) * shadowColor.alphaF() ); // same as breeze.

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
    Q_UNUSED(ev)
}

QSize TerminalHeaderBar::minimumSizeHint() const
{
    auto height = sizeHint().height();
    return {height, height};
}

QSplitter *TerminalHeaderBar::getTopLevelSplitter()
{
    QWidget *p = parentWidget();
    // This is expected.
    if (qobject_cast<TerminalDisplay*>(p)) {
        p = p->parentWidget();
    }

    // this is also expected.
    auto *innerSplitter = qobject_cast<ViewSplitter*>(p);
    if (!innerSplitter) {
        return nullptr;
    }

    return innerSplitter->getToplevelSplitter();
}

void TerminalHeaderBar::applyVisibilitySettings()
{
    auto *settings = KonsoleSettings::self();
    auto toVisibility = settings->splitViewVisibility();
    switch (toVisibility)
    {
    case KonsoleSettings::AlwaysShowSplitHeader:
        setVisible(true);
    break;
    case KonsoleSettings::ShowSplitHeaderWhenNeeded: {
        const bool visible = !(getTopLevelSplitter()->findChildren<TerminalDisplay*>().count() == 1);
        setVisible(visible);
    }
    break;
    case KonsoleSettings::AlwaysHideSplitHeader:
        setVisible(false);
    default:
        break;
    }
}

}
