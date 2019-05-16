#include "TerminalHeaderBar.h"

#include "TerminalDisplay.h"
#include "SessionController.h"

#include <KLocalizedString>
#include <QBoxLayout>
#include <QToolButton>
#include <QMenu>
#include <QLabel>
#include <QAction>

namespace Konsole {

TerminalHeaderBar::TerminalHeaderBar(TerminalDisplay *terminalDisplay, QWidget *parent)
    : QToolBar(parent),
    m_terminalDisplay(terminalDisplay)
{
    auto closeAction = new QAction(
        QIcon::fromTheme(QStringLiteral("tab-close")),
        i18nc("@action:inmenu", "Close terminal"),
        this
    );

    closeAction->setObjectName(
        QStringLiteral("close-terminal")
    );

    addSpacer();
    addAction(closeAction);
}

void TerminalHeaderBar::addSpacer()
{
    // Spacer
    auto *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    addWidget(spacer);
}

}
