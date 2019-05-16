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

namespace Konsole {

TerminalHeaderBar::TerminalHeaderBar(QWidget *parent)
    : QWidget(parent)
{
    m_closeBtn = new QToolButton();
    m_closeBtn->setIcon(QIcon::fromTheme(QStringLiteral("tab-close")));
    m_closeBtn->setToolTip(i18nc("@action:itooltip", "Close terminal"));
    m_closeBtn->setText(i18nc("@action:itooltip", "Close terminal"));
    m_closeBtn->setObjectName(QStringLiteral("close-terminal-button"));
    m_closeBtn->setAutoRaise(true);

    m_terminalTitle = new QLabel();

    m_boxLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    m_boxLayout->setSpacing(0);
    m_boxLayout->setMargin(0);

    // Layout Setup
    m_boxLayout->addStretch();
    m_boxLayout->addWidget(m_terminalTitle);
    m_boxLayout->addStretch();
    m_boxLayout->addWidget(m_closeBtn);
    setLayout(m_boxLayout);
}

// Hack untill I can detangle the creation of the TerminalViews
void TerminalHeaderBar::finishHeaderSetup(ViewProperties *properties)
{
    //TODO: Fix ViewProperties signals.
    connect(properties, &Konsole::ViewProperties::titleChanged, this,
    [this, properties]{
        m_terminalTitle->setText(properties->title());
    });

    /*
    connect(item, &Konsole::ViewProperties::iconChanged, this,
            &Konsole::TabbedViewContainer::updateIcon);

    connect(item, &Konsole::ViewProperties::activity, this,
            &Konsole::TabbedViewContainer::updateActivity);
*/
}


}
