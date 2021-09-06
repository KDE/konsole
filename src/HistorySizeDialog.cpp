/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2012 Kurt Hindenburg <kurt.hindenburg@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "HistorySizeDialog.h"

// Konsole
#include "Shortcut_p.h"
#include "ui_HistorySizeDialog.h"

#include <KLocalizedString>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>

using namespace Konsole;

HistorySizeDialog::HistorySizeDialog(QWidget *parent)
    : QDialog(parent)
    , _ui(nullptr)
{
    setWindowTitle(i18nc("@title:window", "Adjust Scrollback"));
    auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    auto mainWidget = new QWidget(this);
    auto mainLayout = new QVBoxLayout;
    setLayout(mainLayout);
    mainLayout->addWidget(mainWidget);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &HistorySizeDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &HistorySizeDialog::reject);
    mainLayout->addWidget(buttonBox);

    setWindowModality(Qt::WindowModal);

    _ui = new Ui::HistorySizeDialog();
    _ui->setupUi(mainWidget);

    _ui->tempWarningWidget->setVisible(true);
    _ui->tempWarningWidget->setWordWrap(false);
    _ui->tempWarningWidget->setCloseButtonVisible(false);
    _ui->tempWarningWidget->setMessageType(KMessageWidget::Information);
    _ui->tempWarningWidget->setText(i18nc("@info:status", "Any adjustments are only temporary to this session."));
}

HistorySizeDialog::~HistorySizeDialog()
{
    delete _ui;
}

void HistorySizeDialog::setMode(Enum::HistoryModeEnum aMode)
{
    _ui->historySizeWidget->setMode(aMode);
}

Enum::HistoryModeEnum HistorySizeDialog::mode() const
{
    return _ui->historySizeWidget->mode();
}

int HistorySizeDialog::lineCount() const
{
    return _ui->historySizeWidget->lineCount();
}

void HistorySizeDialog::setLineCount(int lines)
{
    _ui->historySizeWidget->setLineCount(lines);
}

QSize HistorySizeDialog::sizeHint() const
{
    return {_ui->tempWarningWidget->sizeHint().width(), 0};
}
