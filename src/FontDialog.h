/*
    Copyright 2018 by Mariusz Glebocki <mglb@arccos-1.net>

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

#ifndef FONTDIALOG_H
#define FONTDIALOG_H

// Qt
#include <QDialog>
#include <QCheckBox>
#include <KFontChooser>
#include <QDialogButtonBox>
#include <QToolButton>

namespace Konsole {
class FontDialog: public QDialog
{
    Q_OBJECT

public:
    explicit FontDialog(QWidget *parent = nullptr);

    QFont font() const { return _fontChooser->font(); }
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
