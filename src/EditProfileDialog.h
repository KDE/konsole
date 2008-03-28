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

#ifndef EDITPROFILEDIALOG_H
#define EDITPROFILEDIALOG_H

// Qt
#include <QtGui/QAbstractItemDelegate>
#include <QtCore/QPair>
#include <QtCore/QHash>
#include <QtCore/QSet>
#include <QtCore/QPointer>

// KDE
#include <KDialog>

// Local
#include "Profile.h"

class QAbstractButton;
class QItemSelectionModel;
class QTextCodec;
class QTimeLine;

namespace Ui
{
    class EditProfileDialog;
}

namespace Konsole
{

class Profile;

/**
 * A dialog which allows the user to edit a profile.
 * After the dialog is created, it can be initialised with the settings
 * for a profile using setProfile().  When the user makes changes to the 
 * dialog and accepts the changes, the dialog will update the
 * profile in the SessionManager by calling the SessionManager's 
 * changeProfile() method.
 *
 * Some changes made in the dialog are preview-only changes which cause
 * the SessionManager's changeProfile() method to be called with
 * the persistant argument set to false.  These changes are then
 * un-done when the dialog is closed.
 */
class EditProfileDialog : public KDialog
{
Q_OBJECT

public:
    /** Constructs a new dialog with the specified parent. */
    EditProfileDialog(QWidget* parent = 0);
    virtual ~EditProfileDialog();

    /**
     * Initialises the dialog with the settings for the specified session
     * type.
     *
     * When the dialog closes, the profile will be updated in the SessionManager
     * with the altered settings.
     *
     * @param key The key for the session type provided by the SessionManager instance
     */
    void setProfile(Profile::Ptr profile);

    /** 
     * Selects the text in the profile name edit area. 
     * When the dialog is being used to create a new profile,
     * this can be used to draw the user's attention to the profile name
     * and make it easy for them to change it.
     */
    void selectProfileName();

public slots:
    // reimplemented
    virtual void accept();
    // reimplemented 
    virtual void reject();

protected:
    virtual bool eventFilter(QObject* watched , QEvent* event);

private slots:
    // sets up the specified tab page if necessary
    void preparePage(int);

    // saves changes to profile
    void save();

    // general page
    void selectInitialDir();
    void selectIcon();

    void profileNameChanged(const QString& text);
    void initialDirChanged(const QString& text);
	void startInSameDir(bool);
    void commandChanged(const QString& text);
    void tabTitleFormatChanged(const QString& text);
    void remoteTabTitleFormatChanged(const QString& text);

    void insertTabTitleText(const QString& text);
    void insertRemoteTabTitleText(const QString& text);

    void showMenuBar(bool);
    void showEnvironmentEditor();
    void tabBarVisibilityChanged(int);
    void tabBarPositionChanged(int);

    // appearance page
    void setFontSize(int pointSize);
	void setAntialiasText(bool enable);
    void showFontDialog();
    void newColorScheme();
    void editColorScheme();
    void removeColorScheme();
    void colorSchemeSelected();
    void previewColorScheme(const QModelIndex& index);
    void fontSelected(const QFont&);

    void colorSchemeAnimationUpdate();

    // scrolling page
    void noScrollBack();
    void fixedScrollBack();
    void unlimitedScrollBack();
   
    void scrollBackLinesChanged(int);

    void hideScrollBar();
    void showScrollBarLeft();
    void showScrollBarRight();

    // keyboard page
    void editKeyBinding();
    void newKeyBinding();
    void keyBindingSelected();
    void removeKeyBinding();

    // advanced page
    void toggleBlinkingText(bool);
    void toggleFlowControl(bool);
    void toggleResizeWindow(bool);
    void toggleBlinkingCursor(bool);

    void setCursorShape(int);
    void autoCursorColor();
    void customCursorColor();
    void customCursorColorChanged(const QColor&);
    void wordCharactersChanged(const QString&);
    void setDefaultCodec(QTextCodec*);

    // apply the first previewed changes stored up by delayedPreview()
    void delayedPreviewActivate();

private:
    // initialize various pages of the dialog
    void setupGeneralPage(const Profile::Ptr info);
    void setupTabsPage(const Profile::Ptr info);
    void setupAppearancePage(const Profile::Ptr info);
    void setupKeyboardPage(const Profile::Ptr info);
    void setupScrollingPage(const Profile::Ptr info);
    void setupAdvancedPage(const Profile::Ptr info);

    void updateColorSchemeList(bool selectCurrentScheme = false);
    void updateColorSchemeButtons();
    void updateKeyBindingsList(bool selectCurrentTranslator = false);
    void updateKeyBindingsButtons();

    void showColorSchemeEditor(bool newScheme);
    void showKeyBindingEditor(bool newTranslator);

    void changeCheckedItem( QAbstractItemModel* mode,  const QModelIndex& to );

    void preview(int property , const QVariant& value);
    void delayedPreview(int property , const QVariant& value);
    void unpreview(int property);
    void unpreviewAll();
    void enableIfNonEmptySelection(QWidget* widget,QItemSelectionModel* selectionModel);

    void updateCaption(const QString& profileName);
    void updateTransparencyWarning();

    struct RadioOption
    {
       QAbstractButton* button;
       int property;
       const char* slot; 
    };
    void setupRadio(RadioOption* possible,int actual);
    struct ComboOption
    {
       QAbstractButton* button;
       int property;
       const char* slot;
    };
    void setupCombo(ComboOption* options , const Profile::Ptr profile);

    const Profile::Ptr lookupProfile() const;

    Ui::EditProfileDialog* _ui;
    Profile::Ptr _tempProfile;
    Profile::Ptr _profileKey;

    // keeps track of pages which need to be updated to match the current
    // profile.  all elements in this vector are set to true when the 
    // profile is changed and individual elements are set to false 
    // after an update by a call to ensurePageLoaded()
    QVector<bool> _pageNeedsUpdate;
    QHash<int,QVariant> _previewedProperties;
    
    QTimeLine* _colorSchemeAnimationTimeLine;

    QHash<int,QVariant> _delayedPreviewProperties;
    QTimer* _delayedPreviewTimer;
};

/**
 * A delegate which can display and edit color schemes in a view.
 */
class ColorSchemeViewDelegate : public QAbstractItemDelegate
{
Q_OBJECT

public:
    ColorSchemeViewDelegate(QObject* parent = 0);

    // reimplemented
    virtual void paint(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;
    virtual QSize sizeHint( const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;

    /** 
     * Sets the timeline used to control the entry animation
     * for this delegate.
     *
     * During a call to paint(), the value of the timeLine is used to
     * determine how to render the item ( with 0 being the beginning 
     * of the animation and 1.0 being the end )
     */
    void setEntryTimeLine( QTimeLine* timeLine );

private:
    QPointer<QTimeLine> _entryTimeLine;

};

}

#endif // EDITPROFILEDIALOG_H
