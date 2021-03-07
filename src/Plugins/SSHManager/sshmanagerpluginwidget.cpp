#include "sshmanagerpluginwidget.h"

#include "ui_sshwidget.h"

struct SSHManagerTreeWidget::Private {

};

SSHManagerTreeWidget::SSHManagerTreeWidget(QWidget *parent)
: QWidget(parent),
ui(std::make_unique<Ui::SSHTreeWidget>()),
d(std::make_unique<SSHManagerTreeWidget::Private>())
{
    ui->setupUi(this);

    connect(ui->newSSHConfig, &QPushButton::clicked, this, &SSHManagerTreeWidget::showInfoPane);
    connect(ui->btnAdd, &QPushButton::clicked, this, &SSHManagerTreeWidget::addSshInfo);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SSHManagerTreeWidget::hideInfoPane);

    hideInfoPane();
}

SSHManagerTreeWidget::~SSHManagerTreeWidget() = default;

void SSHManagerTreeWidget::addSshInfo()
{
    hideInfoPane();
}

void SSHManagerTreeWidget::clearSshInfo()
{
    hideInfoPane();
    ui->name->setText({});
    ui->hostname->setText({});
    ui->port->setText({});
    ui->sshkey->setText({});
}

void SSHManagerTreeWidget::hideInfoPane()
{
    ui->newSSHConfig->show();
    ui->sshInfoPane->hide();
    ui->btnAdd->hide();
    ui->btnCancel->hide();
}

void SSHManagerTreeWidget::showInfoPane()
{
    ui->newSSHConfig->hide();
    ui->sshInfoPane->show();
    ui->btnAdd->show();
    ui->btnCancel->show();
}
