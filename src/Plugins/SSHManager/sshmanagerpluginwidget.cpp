#include "sshmanagerpluginwidget.h"

#include "sshmanagermodel.h"
#include "sshconfigurationdata.h"
#include "session/SessionController.h"

#include "profile/ProfileModel.h"

#include "ui_sshwidget.h"

#include <KLocalizedString>

#include <QAction>
#include <QRegularExpression>
#include <QIntValidator>
#include <QItemSelectionModel>
#include <QRegularExpressionValidator>
#include <QMessageBox>
#include <QPoint>
#include <QMenu>
#include <QDebug>

struct SSHManagerTreeWidget::Private {
    SSHManagerModel *model = nullptr;
    Konsole::SessionController *controller = nullptr;
};

SSHManagerTreeWidget::SSHManagerTreeWidget(QWidget *parent)
: QWidget(parent),
ui(std::make_unique<Ui::SSHTreeWidget>()),
d(std::make_unique<SSHManagerTreeWidget::Private>())
{
    ui->setupUi(this);
    ui->errorPanel->hide();

    // https://stackoverflow.com/questions/1418423/the-hostname-regex
    const auto hostnameRegex = QRegularExpression(
        QStringLiteral(R"(^[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$)")
    );

    const auto* hostnameValidator = new QRegularExpressionValidator(hostnameRegex);
    ui->hostname->setValidator(hostnameValidator);

    const auto* portValidator = new QIntValidator(0, 9999);
    ui->port->setValidator(portValidator);

    connect(ui->newSSHConfig, &QPushButton::clicked, this, &SSHManagerTreeWidget::showInfoPane);
    connect(ui->btnAdd, &QPushButton::clicked, this, &SSHManagerTreeWidget::addSshInfo);
    connect(ui->btnCancel, &QPushButton::clicked, this, &SSHManagerTreeWidget::hideInfoPane);

    ui->profile->setModel(Konsole::ProfileModel::instance());

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->treeView, &QTreeView::customContextMenuRequested, [this](const QPoint& pos){
        if (!ui->treeView->indexAt(pos).isValid()) {
            return;
        }

        QMenu *menu = new QMenu(this);
        auto action = new QAction(QStringLiteral("Remove"), ui->treeView);
        menu->addAction(action);

        connect(action, &QAction::triggered, this, &SSHManagerTreeWidget::triggerRemove);

        menu->popup(ui->treeView->viewport()->mapToGlobal(pos));
    });

    hideInfoPane();
}

SSHManagerTreeWidget::~SSHManagerTreeWidget() = default;

void SSHManagerTreeWidget::addSshInfo()
{
    SSHConfigurationData data;
    bool error = false;
    QString errorString = QStringLiteral("<ul>");
    const QString li = QStringLiteral("<li>");
    const QString il = QStringLiteral("</li>");

    if (ui->hostname->text().isEmpty()) {
        error = true;
        errorString += li +  i18n("Missing Hostname") + il;
    }

    if (ui->name->text().isEmpty()) {
        error = true;
        errorString += li +  i18n("Missing Name") + il;
    }

    if (ui->port->text().isEmpty()) {
        error = true;
        errorString += li +  i18n("Missing Port") + il;
    }

    if (ui->sshkey->text().isEmpty()) {
        error = true;
        errorString += li +  i18n("Missing SSH Key") + il;
    }

    if (ui->folder->currentText().isEmpty()) {
        error = true;
        errorString += li +  i18n("Missing Folder") + il;
    }

    if (ui->profile->currentText().isEmpty()) {
        error = true;
        errorString += li + i18n("An SSH session must have a profile") + il;
    }
    errorString += QStringLiteral("</ul>");

    if (error) {
        ui->errorPanel->setText(errorString);
        ui->errorPanel->show();
        return;
    }

    data.host = ui->hostname->text();
    data.name = ui->name->text();
    data.port = ui->port->text();
    data.sshKey = ui->sshkey->text();
    data.profileName = ui->profile->currentText();

    // TODO: Allow a way to specify the Parent.
    d->model->addChildItem(data, ui->folder->currentText());

    hideInfoPane();
}

void SSHManagerTreeWidget::triggerRemove()
{
    auto selection =  ui->treeView->selectionModel()->selectedIndexes();
    if (selection.empty()) {
        return;
    }

    if (QMessageBox::warning(this, i18n("Remove SSH Configurations"), i18n("You are about to remove ssh configurations, are you sure?")) == QMessageBox::Cancel) {
        return;
    }

    d->model->removeIndex(selection.at(0));
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

void SSHManagerTreeWidget::setCurrentController(Konsole::SessionController *controller)
{
    d->controller = controller;
}
