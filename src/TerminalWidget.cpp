#include "TerminalWidget.h"

#include "TerminalHeaderBar.h"
#include "TerminalDisplay.h"
#include "ViewProperties.h"
#include "SessionController.h"

#include <QBoxLayout>

namespace Konsole {
TerminalWidget::TerminalWidget(uint randomSeed, QWidget *parent)
    : QWidget(parent)
{
    m_terminalDisplay = new TerminalDisplay();
    m_terminalDisplay->setRandomSeed(randomSeed * 31);

    m_headerBar = new TerminalHeaderBar(m_terminalDisplay);

    auto internalLayout = new QBoxLayout(QBoxLayout::TopToBottom);
    internalLayout->addWidget(m_headerBar);
    internalLayout->addWidget(m_terminalDisplay);
    internalLayout->setSpacing(0);
    internalLayout->setMargin(0);
    setLayout(internalLayout);
}

void TerminalWidget::finishTerminalSetup()
{
    m_headerBar->finishHeaderSetup(m_terminalDisplay->sessionController());
}

TerminalDisplay *TerminalWidget::terminalDisplay() const {
    return m_terminalDisplay;
}

TerminalHeaderBar *TerminalWidget::headerBar() const {
    return m_headerBar;
}

} // namespace Konsole
