/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2012 by Jekyll Wu <adaptee@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301  USA.
*/

// Own
#include "HistorySizeWidget.h"

// Qt
#include <QButtonGroup>
#include <QAbstractButton>
#include <QWhatsThis>

#include <KLocalizedString>

// Konsole
#include "ui_HistorySizeWidget.h"

using namespace Konsole;

HistorySizeWidget::HistorySizeWidget(QWidget *parent) :
    QWidget(parent),
    _ui(nullptr)
{
    _ui = new Ui::HistorySizeWidget();
    _ui->setupUi(this);

    // focus and select the spinner automatically when appropriate
    _ui->fixedSizeHistoryButton->setFocusProxy(_ui->historyLineSpinner);
    connect(_ui->fixedSizeHistoryButton, &QRadioButton::clicked,
            _ui->historyLineSpinner,
            &KPluralHandlingSpinBox::selectAll);

    auto modeGroup = new QButtonGroup(this);
    modeGroup->addButton(_ui->noHistoryButton);
    modeGroup->addButton(_ui->fixedSizeHistoryButton);
    modeGroup->addButton(_ui->unlimitedHistoryButton);
    connect(modeGroup,
            static_cast<void (QButtonGroup::*)(QAbstractButton *)>(&QButtonGroup::buttonClicked),
            this, &Konsole::HistorySizeWidget::buttonClicked);

    _ui->historyLineSpinner->setSuffix(ki18ncp("@label:textbox Unit of scrollback", " line", " lines"));
    setLineCount(HistorySizeWidget::DefaultLineCount);

    connect(_ui->historyLineSpinner,
            static_cast<void (KPluralHandlingSpinBox::*)(int)>(&KPluralHandlingSpinBox::valueChanged),
            this, &Konsole::HistorySizeWidget::historySizeChanged);

    auto warningButtonSizePolicy = _ui->fixedSizeHistoryWarningButton->sizePolicy();
    warningButtonSizePolicy.setRetainSizeWhenHidden(true);

    _ui->fixedSizeHistoryWarningButton->setSizePolicy(warningButtonSizePolicy);
    _ui->fixedSizeHistoryWarningButton->hide();
    connect(_ui->fixedSizeHistoryButton, &QAbstractButton::toggled, _ui->historyLineSpinner, &QWidget::setEnabled);
    connect(_ui->fixedSizeHistoryButton, &QAbstractButton::toggled, _ui->fixedSizeHistoryWarningButton, &QWidget::setVisible);
    connect(_ui->fixedSizeHistoryWarningButton, &QToolButton::clicked, this, [this](bool) {
                const QString message = i18nc("@info:whatsthis", "When using this option, the scrollback data will be saved to RAM. If you choose a huge value, your system may run out of free RAM and cause serious issues with your system.");
                const QPoint pos = QPoint(_ui->fixedSizeHistoryWrapper->width() / 2, _ui->fixedSizeHistoryWrapper->height());
                QWhatsThis::showText(_ui->fixedSizeHistoryWrapper->mapToGlobal(pos), message, _ui->fixedSizeHistoryWrapper);
            });

    _ui->unlimitedHistoryWarningButton->setSizePolicy(warningButtonSizePolicy);
    _ui->unlimitedHistoryWarningButton->hide();
    connect(_ui->unlimitedHistoryButton, &QAbstractButton::toggled, _ui->unlimitedHistoryWarningButton, &QWidget::setVisible);
    connect(_ui->unlimitedHistoryWarningButton, &QToolButton::clicked, this, [this](bool) {
                const auto message = xi18nc("@info:tooltip", "When using this option, the scrollback data will be written unencrypted to temporary files. Those temporary files will be deleted automatically when Konsole is closed in a normal manner.<nl/>Use <emphasis>Settings → Configure Konsole → File Location</emphasis> to select the location of the temporary files.");
                const QPoint pos = QPoint(_ui->unlimitedHistoryWrapper->width() / 2, _ui->unlimitedHistoryWrapper->height());
                QWhatsThis::showText(_ui->unlimitedHistoryWrapper->mapToGlobal(pos), message, _ui->unlimitedHistoryWrapper);
            });

    // Make radio buttons height equal
    // fixedSizeHistoryWrapper contains radio + spinbox + toolbutton, so it
    // has height always equal to or larger than single radio button, and
    // radio + toolbutton
    const int radioButtonHeight = _ui->fixedSizeHistoryWrapper->sizeHint().height();
    _ui->noHistoryButton->setMinimumHeight(radioButtonHeight);
    _ui->unlimitedHistoryButton->setMinimumHeight(radioButtonHeight);
}

HistorySizeWidget::~HistorySizeWidget()
{
    delete _ui;
}

void HistorySizeWidget::buttonClicked(QAbstractButton *)
{
    Enum::HistoryModeEnum selectedMode = mode();
    emit historyModeChanged(selectedMode);
}

void HistorySizeWidget::setMode(Enum::HistoryModeEnum aMode)
{
    if (aMode == Enum::NoHistory) {
        _ui->noHistoryButton->setChecked(true);
    } else if (aMode == Enum::FixedSizeHistory) {
        _ui->fixedSizeHistoryButton->setChecked(true);
    } else if (aMode == Enum::UnlimitedHistory) {
        _ui->unlimitedHistoryButton->setChecked(true);
    }
}

Enum::HistoryModeEnum HistorySizeWidget::mode() const
{
    if (_ui->noHistoryButton->isChecked()) {
        return Enum::NoHistory;
    } else if (_ui->fixedSizeHistoryButton->isChecked()) {
        return Enum::FixedSizeHistory;
    } else if (_ui->unlimitedHistoryButton->isChecked()) {
        return Enum::UnlimitedHistory;
    }

    Q_ASSERT(false);
    return Enum::NoHistory;
}

void HistorySizeWidget::setLineCount(int lines)
{
    _ui->historyLineSpinner->setValue(lines);
    _ui->historyLineSpinner->setSingleStep(lines / 10);
}

int HistorySizeWidget::lineCount() const
{
    return _ui->historyLineSpinner->value();
}

int HistorySizeWidget::preferredLabelHeight()
{
    Q_ASSERT(_ui);
    Q_ASSERT(_ui->noHistoryButton);

    return _ui->fixedSizeHistoryWrapper->sizeHint().height();
}
