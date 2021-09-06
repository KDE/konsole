/*
    SPDX-FileCopyrightText: 2018 Mariusz Glebocki <mglb@arccos-1.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef FONTDIALOG_H
#define FONTDIALOG_H

// Qt
#include <KFontChooser>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QToolButton>

namespace Konsole
{
class FontDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FontDialog(QWidget *parent = nullptr);

    QFont font() const
    {
        return _fontChooser->font();
    }
    void setFont(const QFont &font);

Q_SIGNALS:
    void fontChanged(const QFont &font);

private:
    KFontChooser *_fontChooser;
    QCheckBox *_showAllFonts;
    QToolButton *_showAllFontsWarningButton;
    QDialogButtonBox *_buttonBox;
};
}

#endif // FONTDIALOG_H
