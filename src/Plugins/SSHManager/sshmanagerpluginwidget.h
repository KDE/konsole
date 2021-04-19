#ifndef SSSHMANAGERPLUGINWIDGET_H
#define SSSHMANAGERPLUGINWIDGET_H

#include <QWidget>
#include <memory>
namespace Ui {
    class SSHTreeWidget;
}

namespace Konsole {
    class SessionController;
}
class SSHManagerModel;

class SSHManagerTreeWidget : public QWidget {
    Q_OBJECT
public:
    SSHManagerTreeWidget(QWidget *parent = nullptr);
    ~SSHManagerTreeWidget();

    Q_SLOT void showInfoPane();
    Q_SLOT void hideInfoPane();
    Q_SLOT void addSshInfo();
    Q_SLOT void clearSshInfo();

    void setModel(SSHManagerModel *model);
    void triggerRemove();
    void setCurrentController(Konsole::SessionController *controller);
    void connectRequested(const QModelIndex& idx);

private:
    struct Private;
    std::unique_ptr<Ui::SSHTreeWidget> ui;
    std::unique_ptr<Private> d;
};

#endif
