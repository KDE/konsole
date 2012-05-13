/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>

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
#include "ColorSchemeEditor.h"

// Qt
#include <QtGui/QFontMetrics>
#include <QtCore/QFileInfo>

// KDE
#include <KColorDialog>
#include <KWindowSystem>
#include <KFileDialog>
#include <KUrlCompletion>

// Konsole
#include "ui_ColorSchemeEditor.h"
#include "ColorScheme.h"
#include "CharacterColor.h"

using namespace Konsole;

ColorSchemeEditor::ColorSchemeEditor(QWidget* aParent)
    : QWidget(aParent)
    , _colors(0)
{
    _ui = new Ui::ColorSchemeEditor();
    _ui->setupUi(this);

    // description edit
    _ui->descriptionEdit->setClearButtonShown(true);
    connect(_ui->descriptionEdit , SIGNAL(textChanged(QString)) , this ,
            SLOT(setDescription(QString)));

    // transparency slider
    QFontMetrics metrics(font());
    _ui->transparencyPercentLabel->setMinimumWidth(metrics.width("100%"));

    connect(_ui->transparencySlider , SIGNAL(valueChanged(int)) , this , SLOT(setTransparencyPercentLabel(int)));

    // randomized background
    connect(_ui->randomizedBackgroundCheck , SIGNAL(toggled(bool)) , this ,
            SLOT(setRandomizedBackgroundColor(bool)));

    // wallpaper stuff
    KUrlCompletion* fileCompletion = new KUrlCompletion(KUrlCompletion::FileCompletion);
    fileCompletion->setParent(this);
    _ui->wallpaperPath->setCompletionObject(fileCompletion);
    _ui->wallpaperPath->setClearButtonShown(true);
    _ui->wallpaperSelectButton->setIcon(KIcon("image-x-generic"));

    connect(_ui->wallpaperSelectButton, SIGNAL(clicked()),
            this, SLOT(selectWallpaper()));
    connect(_ui->wallpaperPath, SIGNAL(textChanged(QString)),
            this, SLOT(wallpaperPathChanged(QString)));

    // color table
    _ui->colorTable->setColumnCount(2);
    _ui->colorTable->setRowCount(TABLE_COLORS);

    QStringList labels;
    labels << i18nc("@label:listbox Column header text for color names", "Name") << i18nc("@label:listbox Column header text for the actual colors", "Color");
    _ui->colorTable->setHorizontalHeaderLabels(labels);

    _ui->colorTable->horizontalHeader()->setStretchLastSection(true);

    QTableWidgetItem* item = new QTableWidgetItem("Test");
    _ui->colorTable->setItem(0, 0, item);

    _ui->colorTable->verticalHeader()->hide();

    connect(_ui->colorTable , SIGNAL(itemClicked(QTableWidgetItem*)) , this ,
            SLOT(editColorItem(QTableWidgetItem*)));

    // warning label when transparency is not available
    _ui->transparencyWarningWidget->setWordWrap(true);
    _ui->transparencyWarningWidget->setCloseButtonVisible(false);
    _ui->transparencyWarningWidget->setMessageType(KMessageWidget::Warning);

    if (KWindowSystem::compositingActive()) {
        _ui->transparencyWarningWidget->setVisible(false);
    } else {
        _ui->transparencyWarningWidget->setText(i18nc("@info:status",
                                                "The background transparency setting will not"
                                                " be used because your desktop does not appear to support"
                                                " transparent windows."));
    }
}
ColorSchemeEditor::~ColorSchemeEditor()
{
    delete _colors;
    delete _ui;
}
void ColorSchemeEditor::editColorItem(QTableWidgetItem* item)
{
    const int COLOR_COLUMN = 1;

    // ignore if this is not a color column
    if (item->column() != COLOR_COLUMN)
        return;

    QColor color = item->background().color();
    int result = KColorDialog::getColor(color);
    if (result == KColorDialog::Accepted) {
        item->setBackground(color);

        ColorEntry entry(_colors->colorEntry(item->row()));
        entry.color = color;
        _colors->setColorTableEntry(item->row(), entry);

        emit colorsChanged(_colors);
    }
}
void ColorSchemeEditor::selectWallpaper()
{
    const KUrl url = KFileDialog::getImageOpenUrl(_ui->wallpaperPath->text(),
                     this,
                     i18nc("@action:button", "Select wallpaper image file"));

    if (!url.isEmpty())
        _ui->wallpaperPath->setText(url.path());
}
void ColorSchemeEditor::wallpaperPathChanged(const QString& path)
{
    if (path.isEmpty()) {
        _colors->setWallpaper(path);
    } else {
        QFileInfo i(path);

        if (i.exists() && i.isFile() && i.isReadable())
            _colors->setWallpaper(path);
    }
}
void ColorSchemeEditor::setDescription(const QString& text)
{
    if (_colors)
        _colors->setDescription(text);

    if (_ui->descriptionEdit->text() != text)
        _ui->descriptionEdit->setText(text);
}
void ColorSchemeEditor::setTransparencyPercentLabel(int percent)
{
    _ui->transparencyPercentLabel->setText(QString("%1%").arg(percent));

    const qreal opacity = (100.0 - percent) / 100.0;
    _colors->setOpacity(opacity);
}
void ColorSchemeEditor::setRandomizedBackgroundColor(bool randomize)
{
    _colors->setRandomizedBackgroundColor(randomize);
}
void ColorSchemeEditor::setup(const ColorScheme* scheme)
{
    delete _colors;

    _colors = new ColorScheme(*scheme);

    // setup description edit
    _ui->descriptionEdit->setText(_colors->description());

    // setup color table
    setupColorTable(_colors);

    // setup transparency slider
    const int transparencyPercent = qRound((1 - _colors->opacity()) * 100);
    _ui->transparencySlider->setValue(transparencyPercent);
    setTransparencyPercentLabel(transparencyPercent);

    // randomized background color checkbox
    _ui->randomizedBackgroundCheck->setChecked(scheme->randomizedBackgroundColor());

    // wallpaper stuff
    _ui->wallpaperPath->setText(scheme->wallpaper()->path());
}
void ColorSchemeEditor::setupColorTable(const ColorScheme* colors)
{
    ColorEntry table[TABLE_COLORS];
    colors->getColorTable(table);

    for (int row = 0 ; row < TABLE_COLORS ; row++) {
        QTableWidgetItem* nameItem = new QTableWidgetItem(ColorScheme::translatedColorNameForIndex(row));
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);

        QTableWidgetItem* colorItem = new QTableWidgetItem();
        colorItem->setBackground(table[row].color);
        colorItem->setFlags(colorItem->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable);
        colorItem->setToolTip(i18nc("@info:tooltip", "Click to choose color"));

        _ui->colorTable->setItem(row, 0, nameItem);
        _ui->colorTable->setItem(row, 1, colorItem);
    }

    // ensure that color names are as fully visible as possible
    _ui->colorTable->resizeColumnToContents(0);
}
ColorScheme* ColorSchemeEditor::colorScheme() const
{
    return _colors;
}

#include "ColorSchemeEditor.moc"
