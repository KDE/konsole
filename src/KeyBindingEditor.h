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

#ifndef KEYBINDINGEDITOR_H
#define KEYBINDINGEDITOR_H

// Qt
#include <QtGui/QWidget>

class QTableWidgetItem;

namespace Ui
{
    class KeyBindingEditor;
}

namespace Konsole
{

class KeyboardTranslator;

/**
 * A dialog which allows the user to edit a key bindings list 
 * which maps between key combinations input by the user and 
 * the character sequence sent to the terminal when those 
 * combinations are pressed.
 *
 * The dialog can be initialised with the settings of an
 * existing key bindings list using the setup() method.
 *
 * The dialog creates a copy of the supplied keyboard translator
 * to which any changes are applied.  The modified translator 
 * can be retrieved using the translator() method.
 */
class KeyBindingEditor : public QWidget
{
Q_OBJECT

public:
    /** Constructs a new key bindings editor with the specified parent. */
    KeyBindingEditor(QWidget* parent = 0);
    virtual ~KeyBindingEditor();

    /** 
     * Intialises the dialog with the bindings and other settings
     * from the specified @p translator.
     */
    void setup(const KeyboardTranslator* translator);

    /**
     * Returns the modified translator describing the changes to the bindings
     * and other settings which the user made.
     */
    KeyboardTranslator* translator() const;

    /**
     * Returns the text of the editor's description field.
     */
    QString description() const;

    // reimplemented to handle test area input
    virtual bool eventFilter( QObject* watched , QEvent* event );

public slots:
    /** 
     * Sets the text of the editor's description field.
     */
    void setDescription(const QString& description);

private slots:
    void bindingTableItemChanged(QTableWidgetItem* item);
    void removeSelectedEntry();
    void addNewEntry();

private:
    void setupKeyBindingTable(const KeyboardTranslator* translator);

    Ui::KeyBindingEditor* _ui;
    
    // translator to which modifications are made as the user makes
    // changes in the UI.
    // this is initialized as a copy of the translator specified
    // when setup() is called
    KeyboardTranslator* _translator;
};

}

#endif //KEYBINDINGEDITOR_H
