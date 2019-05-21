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
#include <QMouseEvent>
#include <QMimeData>
#include <QDrag>

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
    m_toggleExpandedMode->setIcon(QIcon::fromTheme(QStringLiteral("format-font-size-more"))); // fake 'expand' icon. VDG input?
    m_toggleExpandedMode->setAutoRaise(true);

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
    m_terminalTitle->setEnabled(true);
}

void TerminalHeaderBar::terminalFocusOut()
{
    // TODO: Don't focusOut if searchBar is enabled.
    m_terminalTitle->setEnabled(false);
}

void TerminalHeaderBar::mousePressEvent(QMouseEvent *event)
{
    Q_UNUSED(event)

    auto mimeData = new QMimeData();
    mimeData->setData(QStringLiteral("konsole/terminal_display"), {});

    auto dragAction = new QDrag(this);
    dragAction->setMimeData(mimeData);

    dragAction->exec();
}

}
