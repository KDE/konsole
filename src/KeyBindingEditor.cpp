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

// Qt
#include <QHeaderView>

// Konsole
#include "KeyBindingEditor.h"
#include "ui_KeyBindingEditor.h"
#include "KeyboardTranslator.h"

using namespace Konsole;

KeyBindingEditor::KeyBindingEditor(QWidget* parent)
    : QWidget(parent)
    , _translator(0)
{
    _ui = new Ui::KeyBindingEditor();
    _ui->setupUi(this);

    // key bindings table
    _ui->keyBindingTable->setColumnCount(2);

    QStringList labels;
    labels << i18n("Key Combination") << i18n("Binding");

    _ui->keyBindingTable->setHorizontalHeaderLabels(labels);
    _ui->keyBindingTable->horizontalHeader()->setStretchLastSection(true);
    _ui->keyBindingTable->verticalHeader()->hide();
}

KeyBindingEditor::~KeyBindingEditor()
{
    delete _ui;
}

void KeyBindingEditor::setup(const KeyboardTranslator* translator)
{
    if ( _translator )
        delete _translator;

    _translator = new KeyboardTranslator(*translator);

    // setup description edit
    _ui->descriptionEdit->setText(translator->description());

    // setup key binding table
    setupKeyBindingTable(translator);
}

KeyboardTranslator* KeyBindingEditor::translator() const
{
    return _translator;
}

void KeyBindingEditor::setupKeyBindingTable(const KeyboardTranslator* translator)
{
    QList<KeyboardTranslator::Entry> entries = translator->entries();
    _ui->keyBindingTable->setRowCount(entries.count());
    qDebug() << "Keyboard translator has" << entries.count() << "entries.";

    for ( int row = 0 ; row < entries.count() ; row++ )
    {
        const KeyboardTranslator::Entry& entry = entries.at(row);

        QTableWidgetItem* keyItem = new QTableWidgetItem(entry.keySequence().toString());
        QTableWidgetItem* textItem = new QTableWidgetItem(QString(entry.text()));

        _ui->keyBindingTable->setItem(row,0,keyItem);
        _ui->keyBindingTable->setItem(row,1,textItem);
    }
    _ui->keyBindingTable->sortItems(0);
}

#include "KeyBindingEditor.moc"

