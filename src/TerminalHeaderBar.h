#ifndef TERMINAL_HEADER_BAR_H
#define TERMINAL_HEADER_BAR_H

#include <QToolBar>

class QLabel;
class QToolButton;

namespace Konsole {
    class TerminalDisplay;

class TerminalHeaderBar : public QToolBar {
    Q_OBJECT
public:
    // TODO: Verify if the terminalDisplay is needed, or some other thing like SessionController.
    TerminalHeaderBar(TerminalDisplay *terminalDisplay, QWidget *parent = nullptr);

private:
    void addSpacer();
    TerminalDisplay *m_terminalDisplay;
    QLabel *m_terminalTitle;
    std::vector<QAction*> m_actions;
};

} // namespace Konsole

#endif
