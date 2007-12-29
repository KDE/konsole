/*
    Copyright (C) 2007 by Robert Knight <robertknight@gmail.com>

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
#include <QtGui/QBrush>
#include <QtGui/QFontMetrics>
#include <QtGui/QHeaderView>
#include <QtGui/QItemDelegate>
#include <QtGui/QItemEditorCreator>

// KDE
#include <KColorDialog>
#include <KDebug>
#include <KWindowSystem>

// Konsole
#include "ui_ColorSchemeEditor.h"
#include "CharacterColor.h"

using namespace Konsole;

#if 0
class ColorEditorCreator : public QItemEditorCreatorBase
{
    virtual QWidget* createWidget(QWidget* parent) const
    {
        return new KColorButton(parent);
    }

    virtual QByteArray valuePropertyName() const
    {
        return QByteArray("color");
    }
};
#endif

ColorSchemeEditor::ColorSchemeEditor(QWidget* parent)
    : QWidget(parent)
    , _colors(0)
{
    _ui = new Ui::ColorSchemeEditor();
    _ui->setupUi(this);

    // description edit
    connect( _ui->descriptionEdit , SIGNAL(textChanged(const QString&)) , this , 
            SLOT(setDescription(const QString&)) );

    // transparency slider
    QFontMetrics metrics(font());
    _ui->transparencyPercentLabel->setMinimumWidth( metrics.width("100%") );

    connect( _ui->transparencySlider , SIGNAL(valueChanged(int)) , this , SLOT(setTransparencyPercentLabel(int)) );

    // randomized background
    connect( _ui->randomizedBackgroundCheck , SIGNAL(toggled(bool)) , this , 
             SLOT(setRandomizedBackgroundColor(bool)) );

    // color table
    _ui->colorTable->setColumnCount(2);
    _ui->colorTable->setRowCount(TABLE_COLORS);

    QStringList labels;
    labels << i18n("Name") << i18n("Color");
    _ui->colorTable->setHorizontalHeaderLabels(labels);

    _ui->colorTable->horizontalHeader()->setStretchLastSection(true);

    QTableWidgetItem* item = new QTableWidgetItem("Test");
    _ui->colorTable->setItem(0,0,item);

    _ui->colorTable->verticalHeader()->hide();

    connect( _ui->colorTable , SIGNAL(itemClicked(QTableWidgetItem*)) , this , 
            SLOT(editColorItem(QTableWidgetItem*)) );

    kDebug() << "Color scheme editor - have compositing = " << KWindowSystem::compositingActive();

    // warning label when transparency is not available
    if ( KWindowSystem::compositingActive() )
    {
        _ui->transparencyWarningWidget->setVisible(false);
    }
    else
    {
        _ui->transparencyWarningIcon->setPixmap( KIcon("dialog-warning").pixmap(QSize(48,48)) );
    }
}
void ColorSchemeEditor::setRandomizedBackgroundColor( bool randomize )
{
    _colors->setRandomizedBackgroundColor(randomize);
}
ColorSchemeEditor::~ColorSchemeEditor()
{
    delete _colors;
    delete _ui;
}
void ColorSchemeEditor::editColorItem( QTableWidgetItem* item )
{
    // ignore if this is not a color column
    if ( item->column() != 1 ) 
        return;

    KColorDialog* dialog = new KColorDialog(this);
    dialog->setColor( item->background().color() );

    dialog->exec();

    item->setBackground( dialog->color() );

    ColorEntry entry(_colors->colorEntry(item->row()));
    entry.color = dialog->color();
    _colors->setColorTableEntry(item->row(),entry); 

    emit colorsChanged(_colors);
}
void ColorSchemeEditor::setDescription(const QString& text)
{
    if ( _colors )
        _colors->setDescription(text);

    if ( _ui->descriptionEdit->text() != text )
        _ui->descriptionEdit->setText(text);
}
void ColorSchemeEditor::setTransparencyPercentLabel(int percent)
{
    _ui->transparencyPercentLabel->setText( QString("%1%").arg(percent) );
    
    qreal opacity = ( 100.0 - percent ) / 100.0;
    _colors->setOpacity(opacity);

    //kDebug() << "set opacity to:" << opacity;
}
void ColorSchemeEditor::setup(const ColorScheme* scheme)
{
    if ( _colors )
        delete _colors;

    _colors = new ColorScheme(*scheme);

    // setup description edit
    _ui->descriptionEdit->setText(_colors->description());

    // setup color table
    setupColorTable(_colors);

    // setup transparency slider
    //kDebug() << "read opacity: " << _colors->opacity();
    const int transparencyPercent = (int) ( (1-_colors->opacity())*100 );
    
    _ui->transparencySlider->setValue(transparencyPercent);
    setTransparencyPercentLabel(transparencyPercent);

    // randomized background color checkbox
    _ui->randomizedBackgroundCheck->setChecked( scheme->randomizedBackgroundColor() );
}
void ColorSchemeEditor::setupColorTable(const ColorScheme* colors)
{
    ColorEntry table[TABLE_COLORS];
    colors->getColorTable(table);

    for ( int row = 0 ; row < TABLE_COLORS ; row++ )
    {
        QTableWidgetItem* nameItem = new QTableWidgetItem( ColorScheme::translatedColorNameForIndex(row) );
        QTableWidgetItem* colorItem = new QTableWidgetItem();
        colorItem->setBackground( table[row].color );
        colorItem->setFlags( colorItem->flags() & ~Qt::ItemIsEditable & ~Qt::ItemIsSelectable );

        _ui->colorTable->setItem(row,0,nameItem);
        _ui->colorTable->setItem(row,1,colorItem);
    }

    // ensure that color names are as fully visible as possible
    _ui->colorTable->resizeColumnToContents(0);
}
ColorScheme* ColorSchemeEditor::colorScheme() const
{
    return _colors;
}

#include "ColorSchemeEditor.moc"
