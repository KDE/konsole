#include "sshmanagerpluginwidget.h"

#include "sshmanagermodel.h"
#include "sshconfigurationdata.h"

#include "ui_sshwidget.h"

struct SSHManagerTreeWidget::Private {
    SSHManagerModel *model;
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
    SSHConfigurationData data;
    data.host = ui->hostname->text();
    data.name = ui->name->text();
    data.port = ui->port->text();
    data.sshKey = ui->sshkey->text();

    // TODO: Allow a way to specify the Parent.
    d->model->addChildItem(data, QStringLiteral("Default"));
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

void SSHManagerTreeWidget::setModel(SSHManagerModel* model)
{
    d->model = model;
    ui->treeView->setModel(model);
}
