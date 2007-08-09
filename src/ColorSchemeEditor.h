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

#ifndef COLORSCHEMEEDITOR_H
#define COLORSCHEMEEDITOR_H

// Qt
#include <QtGui/QWidget>

// Konsole
#include "ColorScheme.h"

class QTableWidgetItem;

namespace Ui
{
    class ColorSchemeEditor;
}

namespace Konsole
{

class ColorScheme;

/**
 * A dialog for editing color schemes.
 *
 * After creation, the dialog can be initialised with the settings
 * of a color scheme using the setup() method.
 *
 * The dialog creates a copy of the supplied color scheme to which
 * any changes made are applied.  The modified color scheme
 * can be retrieved using the colorScheme() method.  
 *
 * When changes are made the colorsChanged() signal is emitted.
 */
class ColorSchemeEditor : public QWidget
{
Q_OBJECT

public:
   /** Constructs a new color scheme editor with the specified parent. */
   ColorSchemeEditor(QWidget* parent = 0);
   virtual ~ColorSchemeEditor();

   /** Initialises the dialog with the properties of the specified color scheme. */
   void setup(const ColorScheme* scheme);
   /** Returns the modified color scheme. */
   ColorScheme* colorScheme() const;

signals:
   /** Emitted when the colors in the color scheme change. */
   void colorsChanged(ColorScheme* scheme);

public slots:
    /** Sets the text displayed in the description edit field. */
   void setDescription(const QString& description);

private slots:
   void setTransparencyPercentLabel(int percent);
   void setRandomizedBackgroundColor(bool randomized); 
   void editColorItem(QTableWidgetItem* item);

private:
   void setupColorTable(const ColorScheme* table);

   Ui::ColorSchemeEditor* _ui;
   ColorScheme* _colors;    
};

}

#endif // COLORSCHEMEEDITOR_H 
