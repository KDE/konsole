#include "TerminalHeaderBar.h"

#include "TerminalDisplay.h"

#include <QBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QLabel>

namespace Konsole {

TerminalHeaderBar::TerminalHeaderBar(TerminalDisplay *terminalDisplay, QWidget *parent)
    : QWidget(parent),
    m_terminalDisplay(terminalDisplay)
{
    m_terminalTitle = new QLabel();
    m_closeButton = new QToolButton();

    auto *horizontalLayout = new QBoxLayout(QBoxLayout::Direction::LeftToRight);
    horizontalLayout->addStretch();
    horizontalLayout->addWidget(m_terminalTitle);
    horizontalLayout->addStretch();
    horizontalLayout->addWidget(m_closeButton);
    setLayout(horizontalLayout);
}

}
