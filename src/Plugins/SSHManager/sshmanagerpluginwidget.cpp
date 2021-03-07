#include "sshmanagerpluginwidget.h"

#include "ui_sshwidget.h"

struct SSHManagerTreeWidget::Private {

};

SSHManagerTreeWidget::SSHManagerTreeWidget(QObject *parent)
: ui(std::make_unique<Ui::SSHTreeWidget>()),
d(std::make_unique<SSHManagerTreeWidget::Private>())
{
    ui->setupUi(this);
};

SSHManagerTreeWidget::~SSHManagerTreeWidget() = default;
