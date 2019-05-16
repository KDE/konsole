#ifndef TERMINAL_HEADER_BAR_H
#define TERMINAL_HEADER_BAR_H

#include <QWidget>

class QLabel;
class QToolButton;
class QBoxLayout;
namespace Konsole {
    class TerminalDisplay;
    class ViewProperties;

class TerminalHeaderBar : public QWidget {
    Q_OBJECT
public:
    // TODO: Verify if the terminalDisplay is needed, or some other thing like SessionController.
    TerminalHeaderBar(QWidget *parent = nullptr);
    void finishHeaderSetup(ViewProperties *properties);

private:
    void addSpacer();
    QBoxLayout *m_boxLayout;
    TerminalDisplay *m_terminalDisplay;
    QLabel *m_terminalTitle;
    QToolButton *m_closeBtn;
};

} // namespace Konsole

#endif
