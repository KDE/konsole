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
#include "KeyBindingEditor.h"

// Qt
#include <QtGui/QHeaderView>
#include <QtGui/QKeyEvent>

// Konsole
#include "ui_KeyBindingEditor.h"
#include "KeyboardTranslator.h"

using namespace Konsole;

KeyBindingEditor::KeyBindingEditor(QWidget* parent)
    : QWidget(parent)
    , _translator(0)
{
    _ui = new Ui::KeyBindingEditor();
    _ui->setupUi(this);

    // description edit
    connect( _ui->descriptionEdit , SIGNAL(textChanged(const QString&)) , this , SLOT(setDescription(const QString&)) );

    // key bindings table
    _ui->keyBindingTable->setColumnCount(2);

    QStringList labels;
    labels << i18n("Key Combination") << i18n("Binding");

    _ui->keyBindingTable->setHorizontalHeaderLabels(labels);
    _ui->keyBindingTable->horizontalHeader()->setStretchLastSection(true);
    _ui->keyBindingTable->verticalHeader()->hide();

    _ui->testAreaInputEdit->installEventFilter(this);
}

KeyBindingEditor::~KeyBindingEditor()
{
    delete _ui;
}

bool KeyBindingEditor::eventFilter( QObject* watched , QEvent* event )
{
    if ( watched == _ui->testAreaInputEdit )
    {
       if ( event->type() == QEvent::KeyPress )
       {
            QKeyEvent* keyEvent = (QKeyEvent*)event;

            int modifiers = keyEvent->modifiers();
            KeyboardTranslator::Entry entry = _translator->findEntry( keyEvent->key() , 
                                                                      (Qt::KeyboardModifier)modifiers );

            _ui->testAreaInputEdit->setText(entry.conditionToString());
            _ui->testAreaOutputEdit->setText(entry.resultToString());

            //qDebug() << "Entry: " << entry.resultToString();

            keyEvent->accept();
            return true;
       } 
    }
    return false;
}

void KeyBindingEditor::setDescription(const QString& newDescription)
{
    if ( description() != newDescription )
    {
        _ui->descriptionEdit->setText(newDescription);
        
        if ( _translator )
            _translator->setDescription(newDescription);
    }
}
QString KeyBindingEditor::description() const
{
    return _ui->descriptionEdit->text();
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

        QTableWidgetItem* keyItem = new QTableWidgetItem(entry.conditionToString());
        QTableWidgetItem* textItem = new QTableWidgetItem(QString(entry.resultToString()));

        _ui->keyBindingTable->setItem(row,0,keyItem);
        _ui->keyBindingTable->setItem(row,1,textItem);
    }
    _ui->keyBindingTable->sortItems(0);
}

#include "KeyBindingEditor.moc"

