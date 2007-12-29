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

#include <KDebug>

// Konsole
#include "ui_KeyBindingEditor.h"
#include "KeyboardTranslator.h"

using namespace Konsole;

KeyBindingEditor::KeyBindingEditor(QWidget* parent)
    : QWidget(parent)
    , _translator(new KeyboardTranslator( QString() ))
{
    _ui = new Ui::KeyBindingEditor();
    _ui->setupUi(this);

    // description edit
    connect( _ui->descriptionEdit , SIGNAL(textChanged(const QString&)) , this , SLOT(setDescription(const QString&)) );

    // key bindings table
    _ui->keyBindingTable->setColumnCount(2);

    QStringList labels;
    labels << i18n("Key Combination") << i18n("Output");

    _ui->keyBindingTable->setHorizontalHeaderLabels(labels);
    _ui->keyBindingTable->horizontalHeader()->setStretchLastSection(true);
    _ui->keyBindingTable->verticalHeader()->hide();

    // add and remove buttons
    _ui->addEntryButton->setIcon( KIcon("list-add") );
    _ui->removeEntryButton->setIcon( KIcon("list-remove") );

    connect( _ui->removeEntryButton , SIGNAL(clicked()) , this , SLOT(removeSelectedEntry()) );
    connect( _ui->addEntryButton , SIGNAL(clicked()) , this , SLOT(addNewEntry()) );
    
    // test area
    _ui->testAreaInputEdit->installEventFilter(this);
}

KeyBindingEditor::~KeyBindingEditor()
{
    delete _ui;
}

void KeyBindingEditor::removeSelectedEntry()
{
    QListIterator<QTableWidgetItem*> iter( _ui->keyBindingTable->selectedItems() );

    while ( iter.hasNext() )
    {     
        // get the first item in the row which has the entry
        QTableWidgetItem* item = _ui->keyBindingTable->item(iter.next()->row(),0);

        KeyboardTranslator::Entry existing = item->data(Qt::UserRole).
                                                    value<KeyboardTranslator::Entry>();
        
        _translator->removeEntry(existing);
    
        _ui->keyBindingTable->removeRow( item->row() );
    }
}

void KeyBindingEditor::addNewEntry()
{
   _ui->keyBindingTable->insertRow( _ui->keyBindingTable->rowCount() );

   int newRowCount = _ui->keyBindingTable->rowCount();

   // block signals here to avoid triggering bindingTableItemChanged() slot call
   _ui->keyBindingTable->blockSignals(true);

   _ui->keyBindingTable->setItem(newRowCount-1,0,new QTableWidgetItem() );
   _ui->keyBindingTable->setItem(newRowCount-1,1,new QTableWidgetItem() );

   _ui->keyBindingTable->blockSignals(false);

   // make sure user can see new row
   _ui->keyBindingTable->scrollToItem(_ui->keyBindingTable->item(newRowCount-1,0));
}

bool KeyBindingEditor::eventFilter( QObject* watched , QEvent* event )
{
    if ( watched == _ui->testAreaInputEdit )
    {
       if ( event->type() == QEvent::KeyPress )
       {
            QKeyEvent* keyEvent = (QKeyEvent*)event;

            // The state here is currently set to the state that a newly started 
            // terminal in Konsole will be in ( which is also the same as the 
            // state just after a reset ), this has 'Ansi' turned on and all other
            // states off.
            //
            // TODO: It may be useful to be able to specify the state in the 'test input' 
            // area, but preferably not in a way which clutters the UI with lots of 
            // checkboxes.
            //
            const KeyboardTranslator::States states = KeyboardTranslator::AnsiState;

            KeyboardTranslator::Entry entry = _translator->findEntry( keyEvent->key() , 
                                                                      keyEvent->modifiers(), 
                                                                      states );

            if ( !entry.isNull() )
            {
                _ui->testAreaInputEdit->setText(entry.conditionToString());
                _ui->testAreaOutputEdit->setText(entry.resultToString(true,keyEvent->modifiers()));
            }
            else
            {
                _ui->testAreaInputEdit->setText(keyEvent->text());
                _ui->testAreaOutputEdit->setText(keyEvent->text());
            }
            //kDebug() << "Entry: " << entry.resultToString();

            keyEvent->accept();
            return true;
       } 
    }
    return false;
}

void KeyBindingEditor::setDescription(const QString& newDescription)
{
     _ui->descriptionEdit->setText(newDescription);
    
     if ( _translator )
         _translator->setDescription(newDescription);
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

void KeyBindingEditor::bindingTableItemChanged(QTableWidgetItem* item)
{
   QTableWidgetItem* key = _ui->keyBindingTable->item( item->row() , 0 );
   KeyboardTranslator::Entry existing = key->data(Qt::UserRole).value<KeyboardTranslator::Entry>();

   QString condition = key->text();
   QString result = _ui->keyBindingTable->item( item->row() , 1 )->text();

   KeyboardTranslator::Entry entry = KeyboardTranslatorReader::createEntry(condition,result);

   kDebug() << "Created entry: " << entry.conditionToString() << " , " << entry.resultToString();

   _translator->replaceEntry(existing,entry);

    // block signals to prevent this slot from being called repeatedly
   _ui->keyBindingTable->blockSignals(true);

   key->setData(Qt::UserRole,QVariant::fromValue(entry));

   _ui->keyBindingTable->blockSignals(false);
}

void KeyBindingEditor::setupKeyBindingTable(const KeyboardTranslator* translator)
{
    disconnect( _ui->keyBindingTable , SIGNAL(itemChanged(QTableWidgetItem*)) , this , 
            SLOT(bindingTableItemChanged(QTableWidgetItem*)) );

    QList<KeyboardTranslator::Entry> entries = translator->entries();
    _ui->keyBindingTable->setRowCount(entries.count());
    //kDebug() << "Keyboard translator has" << entries.count() << "entries.";

    for ( int row = 0 ; row < entries.count() ; row++ )
    {
        const KeyboardTranslator::Entry& entry = entries.at(row);

        QTableWidgetItem* keyItem = new QTableWidgetItem(entry.conditionToString());
        keyItem->setData( Qt::UserRole , QVariant::fromValue(entry) );

        QTableWidgetItem* textItem = new QTableWidgetItem(QString(entry.resultToString()));

        _ui->keyBindingTable->setItem(row,0,keyItem);
        _ui->keyBindingTable->setItem(row,1,textItem);
    }
    _ui->keyBindingTable->sortItems(0);

    connect( _ui->keyBindingTable , SIGNAL(itemChanged(QTableWidgetItem*)) , this , 
            SLOT(bindingTableItemChanged(QTableWidgetItem*)) );
}

#include "KeyBindingEditor.moc"

