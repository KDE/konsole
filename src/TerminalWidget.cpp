#include "TerminalWidget.h"

#include "TerminalHeaderBar.h"
#include "TerminalDisplay.h"

#include <QBoxLayout>

namespace Konsole {
TerminalWidget::TerminalWidget(Session *session, QWidget *parent)
{
    m_terminalDisplay = new TerminalDisplay();
    m_headerBar = new TerminalHeaderBar(m_terminalDisplay);

    auto internalLayout = new QBoxLayout(QBoxLayout::TopToBottom);
    internalLayout->addWidget(m_headerBar);
    internalLayout->addWidget(m_terminalDisplay);
    setLayout(internalLayout);
}

TerminalDisplay *TerminalWidget::terminalDisplay() const {
    return m_terminalDisplay;
}

TerminalHeaderBar *TerminalWidget::headerBar() const {
    return m_headerBar;
}

} // namespace Konsole