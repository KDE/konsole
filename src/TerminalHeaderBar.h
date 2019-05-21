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

    void terminalFocusIn();
    void terminalFocusOut();
protected:
    void mousePressEvent(QMouseEvent *event) override;

Q_SIGNALS:
    void requestToggleExpansion();

private:
    void addSpacer();
    QBoxLayout *m_boxLayout;
    TerminalDisplay *m_terminalDisplay;
    QLabel *m_terminalTitle;
    QLabel *m_terminalIcon;
    QLabel *m_terminalActivity; // Bell icon.
    QToolButton *m_closeBtn;
    QToolButton *m_toggleExpandedMode;
};

} // namespace Konsole

#endif
