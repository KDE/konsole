#ifndef SSSHMANAGERPLUGINWIDGET_H
#define SSSHMANAGERPLUGINWIDGET_H

#include <QWidget>
#include <memory>
namespace Ui {
    class SSHTreeWidget;
}

class SSHManagerTreeWidget : public QWidget {
    Q_OBJECT
public:
    SSHManagerTreeWidget(QObject *parent = nullptr);
    ~SSHManagerTreeWidget();

private:
    struct Private;
    std::unique_ptr<Ui::SSHTreeWidget> ui;
    std::unique_ptr<Private> d;
};

#endif
