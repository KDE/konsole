#ifndef TERMINAL_HEADER_BAR_H
#define TERMINAL_HEADER_BAR_H

#include <QWidget>

class QLabel;
class QToolButton;

namespace Konsole {
    class TerminalDisplay;

class TerminalHeaderBar : public QWidget {
    Q_OBJECT
public:
    // TODO: Verify if the terminalDisplay is needed, or some other thing like SessionController.
    TerminalHeaderBar(TerminalDisplay *terminalDisplay, QWidget *parent = nullptr);

private:
    TerminalDisplay *m_terminalDisplay;
    QLabel *m_terminalTitle;
    QToolButton *m_closeButton;
};

} // namespace Konsole

#endif
