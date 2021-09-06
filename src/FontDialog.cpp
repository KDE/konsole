/*
    SPDX-FileCopyrightText: 2018 Mariusz Glebocki <mglb@arccos-1.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "FontDialog.h"

// Qt
#include <QBoxLayout>
#include <QWhatsThis>

// KDE
#include <KLocalizedString>
#include <kwidgetsaddons_version.h>

using namespace Konsole;

FontDialog::FontDialog(QWidget *parent)
    : QDialog(parent)
    , _fontChooser(nullptr)
    , _showAllFonts(nullptr)
    , _buttonBox(nullptr)
{
    setWindowTitle(i18nc("@title:window", "Select font"));

#if KWIDGETSADDONS_VERSION >= QT_VERSION_CHECK(5, 86, 0)
    _fontChooser = new KFontChooser(KFontChooser::FixedFontsOnly, this);
#else
    _fontChooser = new KFontChooser(this, KFontChooser::FixedFontsOnly);
#endif

    _showAllFonts = new QCheckBox(i18nc("@action:button", "Show all fonts"), this);
    _showAllFontsWarningButton = new QToolButton(this);
    _buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

    _fontChooser->setSampleText(
        QStringLiteral("0OQ 1Il!| 5S 8B rnm :; ,. \"'` ~-= ({[<>]})\n"
                       "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\n"
                       "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789\n"
                       "abcdefghijklmnopqrstuvwxyz"));
    _showAllFontsWarningButton->setIcon(QIcon::fromTheme(QStringLiteral("emblem-warning")));
    _showAllFontsWarningButton->setAutoRaise(true);

    connect(_fontChooser, &KFontChooser::fontSelected, this, &FontDialog::fontChanged);
    connect(_showAllFonts, &QCheckBox::toggled, this, [this](bool enable) {
        _fontChooser->setFont(_fontChooser->font(), !enable);
    });
    connect(_showAllFontsWarningButton, &QToolButton::clicked, this, [this](bool) {
        const QString message = i18nc("@info:status",
                                      "By its very nature, a terminal program requires font characters that are equal width (monospace). Any non monospaced "
                                      "font may cause display issues. This should not be necessary except in rare cases.");
        const QPoint pos = QPoint(_showAllFonts->width() / 2, _showAllFonts->height());
        QWhatsThis::showText(_showAllFonts->mapToGlobal(pos), message, _showAllFonts);
    });
    connect(_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *showAllFontsLayout = new QHBoxLayout();
    showAllFontsLayout->addWidget(_showAllFonts);
    showAllFontsLayout->addWidget(_showAllFontsWarningButton);
    showAllFontsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding));
    showAllFontsLayout->setContentsMargins(0, 0, 0, 0);
    showAllFontsLayout->setSpacing(0);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(_fontChooser, 1);
    layout->addLayout(showAllFontsLayout);
    layout->addWidget(_buttonBox);
}

void FontDialog::setFont(const QFont &font)
{
    _fontChooser->setFont(font, !_showAllFonts->isChecked());
}
