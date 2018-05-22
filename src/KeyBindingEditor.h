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

#ifndef KEYBINDINGEDITOR_H
#define KEYBINDINGEDITOR_H

// Qt
#include <QDialog>

// Konsole
#include "Profile.h"

class QTableWidgetItem;

namespace Ui {
class KeyBindingEditor;
}

namespace Konsole {
class KeyboardTranslator;

/**
 * A dialog which allows the user to edit a key bindings scheme
 * which maps between key combinations input by the user and
 * the character sequence sent to the terminal when those
 * combinations are pressed.
 *
 * The dialog can be initialized with the settings of an
 * existing key bindings scheme using the setup() method.
 *
 * The dialog creates a copy of the supplied keyboard translator
 * to which any changes are applied.  The modified translator
 * can be retrieved using the translator() method.
 */
class KeyBindingEditor : public QDialog
{
    Q_OBJECT

public:
    /** Constructs a new key bindings editor with the specified parent. */
    explicit KeyBindingEditor(QWidget *parent = nullptr);
    ~KeyBindingEditor() Q_DECL_OVERRIDE;

    /**
     * Initializes the dialog with the bindings and other settings
     * from the specified @p translator.
     * @p currentProfileTranslator the name of the translator set in the
     *                             current profile
     * @p isNewTranslator specifies whether the translator being edited
     *                    is an already existing one or a newly created
     *                    one, defaults to false.
     */
    void setup(const KeyboardTranslator *translator, const QString &currentProfileTranslator,
               bool isNewTranslator = false);

    /**
     * Returns the modified translator describing the changes to the bindings
     * and other settings which the user made.
     */
    KeyboardTranslator *translator() const;

    /**
     * Sets the text of the editor's description field.
     */
    void setDescription(const QString &description);

    /**
     * Returns the text of the editor's description field.
     */
    QString description() const;

    // reimplemented to handle test area input
    bool eventFilter(QObject *watched, QEvent *event) Q_DECL_OVERRIDE;

Q_SIGNALS:
    /**
     * Emitted when the user clicks the OK button to save the changes. This
     * signal is connected to EditProfileDialog::updateKeyBindingsList()
     * to update the key bindings list on the Keyboard tab in the Edit
     * Profile dialog.
     * @p translatorName is the translator that has just been edited
     */
    void updateKeyBindingsListRequest(const QString &translatorName);

    /**
     * Emitted when the user clicks the OK button to save the changes to
     * the translator that's set in the current profile; this signal is
     * connected to EditProfileDialog::updateTempProfileProperty() to
     * request applying the changes to the _tempProfile.
     * @p newTranslatorName is the name of the translator, that has just
     *                      been edited/saved, and which is also the translator
     *                      set in the current Profile
     */
    void updateTempProfileKeyBindingsRequest(Profile::Property, const QString &newTranslatorName);

private Q_SLOTS:
    // reimplemented
    void accept() Q_DECL_OVERRIDE;

    void setTranslatorDescription(const QString &description);
    void bindingTableItemChanged(QTableWidgetItem *item);
    void removeSelectedEntry();
    void addNewEntry();

private:
    Q_DISABLE_COPY(KeyBindingEditor)

    void setupKeyBindingTable(const KeyboardTranslator *translator);

    Ui::KeyBindingEditor *_ui;

    // translator to which modifications are made as the user makes
    // changes in the UI.
    // this is initialized as a copy of the translator specified
    // when setup() is called
    KeyboardTranslator *_translator;

    // Show only the rows that match the text entered by the user in the
    // filter search box
    void filterRows(const QString &text);

    bool _isNewTranslator;

    // The translator set in the current profile
    QString _currentProfileTranslator;

    // Sets the size hint of the dialog to be slightly smaller than the
    // size of EditProfileDialog
    QSize sizeHint() const Q_DECL_OVERRIDE;
};
}

#endif //KEYBINDINGEDITOR_H
