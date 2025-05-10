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

using namespace Konsole;

FontDialog::FontDialog(QWidget *parent, bool emoji, const QFont font)
    : QDialog(parent)
    , _fontChooser(nullptr)
    , _showAllFonts(nullptr)
    , _buttonBox(nullptr)
    , _emoji(emoji)
{
    setWindowTitle(i18nc("@title:window", "Select font"));

    KFontChooser::DisplayFlag onlyFixed = _emoji ? KFontChooser::FixedFontsOnly : KFontChooser::FixedFontsOnly;

    _fontChooser = new KFontChooser(onlyFixed, this);
    if (_emoji) {
        _fontChooser->setFont(font);
        _fontChooser->setFontListItems(KFontChooser::createFontList(0).filter(QStringLiteral("emoji"), Qt::CaseInsensitive));
        _fontChooser->setFont(font);
    }

    _showAllFonts = new QCheckBox(i18nc("@action:button", "Show all fonts"), this);
    _showAllFontsWarningButton = new QToolButton(this);
    _buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, Qt::Horizontal, this);

    if (_emoji) {
        _fontChooser->setSampleText(
            /* clang-format off */
            QStringLiteral(" 🏴🤘🚬🌍🌎🌏🥆💣🗡🔫⚗️⚛️☢️☣️🌿🎱🏧💉💊🕴️📡🤻🦑🇦🇶👩‍🔬🪤🚱✊🏿🔬🧬🏴‍☠️🤽\n"
                           "0123456789\n"
                           "👆🏻 👆🏼 👆🏽 👆🏾 👆🏿     👨‍❤️‍👨 👨‍❤️‍💋‍👨 👩‍👩‍👧‍👧 👩🏻‍🤝‍👨🏿 👨‍👨‍👧‍👦 \U0001F468\u200D\u2764\uFE0F\u200D\U0001F468  \U0001F468\u200D\u2764\u200D\U0001F468\n"
                           "🇧🇲 🇨🇭 🇨🇿 🇪🇺 🇬🇱 🇲🇬 🇲🇹 🇸🇿 🇿🇲"));
        /* clang-format on */
        _showAllFonts->hide();
        _showAllFontsWarningButton->hide();
    } else {
        _fontChooser->setSampleText(
            QStringLiteral("0OQ 1Il!| 5S 8B rnm :; ,. \"'` ~-= ({[<>]})\n"
                           "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~\n"
                           "ABCDEFGHIJKLMNOPQRSTUVWXYZ 0123456789\n"
                           "abcdefghijklmnopqrstuvwxyz"));
        _showAllFontsWarningButton->setIcon(QIcon::fromTheme(QStringLiteral("emblem-warning")));
        _showAllFontsWarningButton->setAutoRaise(true);

        connect(_showAllFonts, &QCheckBox::toggled, this, [this](bool enable) {
            _fontChooser->setFont(_fontChooser->font(), !enable);
        });
        connect(_showAllFontsWarningButton, &QToolButton::clicked, this, [this](bool) {
            const QString message =
                i18nc("@info:status",
                      "By its very nature, a terminal program requires font characters that are equal width (monospace). Any non monospaced "
                      "font may cause display issues. This should not be necessary except in rare cases.");
            const QPoint pos = QPoint(_showAllFonts->width() / 2, _showAllFonts->height());
            QWhatsThis::showText(_showAllFonts->mapToGlobal(pos), message, _showAllFonts);
        });
    }

    auto *showAllFontsLayout = new QHBoxLayout();
    showAllFontsLayout->addWidget(_showAllFonts);
    showAllFontsLayout->addWidget(_showAllFontsWarningButton);
    showAllFontsLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Expanding));
    showAllFontsLayout->setContentsMargins(0, 0, 0, 0);
    showAllFontsLayout->setSpacing(0);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(_fontChooser, 1);
    if (!_emoji) {
        layout->addLayout(showAllFontsLayout);
    }
    layout->addWidget(_buttonBox);
    connect(_fontChooser, &KFontChooser::fontSelected, this, &FontDialog::fontChanged);
    connect(_buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void FontDialog::setFont(const QFont &font)
{
    _fontChooser->setFont(font, !_showAllFonts->isChecked() && !_emoji);
}

#include "moc_FontDialog.cpp"
