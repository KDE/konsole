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
#include <QWidget>

namespace Ui
{
    class KeyBindingEditor;
};

namespace Konsole
{

class KeyboardTranslator;

class KeyBindingEditor : public QWidget
{
Q_OBJECT

public:
    KeyBindingEditor(QWidget* parent = 0);
    virtual ~KeyBindingEditor();

    void setup(const KeyboardTranslator* translator);

    KeyboardTranslator* translator() const;

    QString description() const;

public slots:
    void setDescription(const QString& description);
    
private:
    void setupKeyBindingTable(const KeyboardTranslator* translator);

    Ui::KeyBindingEditor* _ui;
    KeyboardTranslator* _translator;
};

};

#endif //KEYBINDINGEDITOR_H
