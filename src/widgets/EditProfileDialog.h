/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2018 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef EDITPROFILEDIALOG_H
#define EDITPROFILEDIALOG_H

// KDE
#include <KNS3/Entry>
#include <KNSCore/EntryInternal>
#include <KPageDialog>
#include <knewstuff_version.h>

// Konsole
#include "colorscheme/ColorScheme.h"
#include "colorscheme/ColorSchemeEditor.h"
#include "colorscheme/ColorSchemeViewDelegate.h"

#include "profile/Profile.h"
#include "profile/ProfileGroup.h"

#include "Enumeration.h"
#include "FontDialog.h"
#include "LabelsAligner.h"
#include "keyboardtranslator/KeyboardTranslatorManager.h"

class KPluralHandlingSpinBox;
class KLocalizedString;
class QItemSelectionModel;
class QTextCodec;

namespace Ui
{
class EditProfileGeneralPage;
class EditProfileTabsPage;
class EditProfileAppearancePage;
class EditProfileScrollingPage;
class EditProfileKeyboardPage;
class EditProfileMousePage;
class EditProfileAdvancedPage;
}

namespace Konsole
{
/**
 * A dialog which allows the user to edit a profile.
 * After the dialog is created, it can be initialized with the settings
 * for a profile using setProfile().  When the user makes changes to the
 * dialog and accepts the changes, the dialog will update the
 * profile in the SessionManager by calling the SessionManager's
 * changeProfile() method.
 *
 * Some changes made in the dialog are preview-only changes which cause
 * the SessionManager's changeProfile() method to be called with
 * the persistent argument set to false.  These changes are then
 * un-done when the dialog is closed.
 */
class KONSOLEPRIVATE_EXPORT EditProfileDialog : public KPageDialog
{
    Q_OBJECT

public:
    /** Constructs a new dialog with the specified parent. */
    explicit EditProfileDialog(QWidget *parent = nullptr);
    ~EditProfileDialog() override;

    enum InitialProfileState {
        ExistingProfile,
        NewProfile,
    };

    /**
     * Initializes the dialog with the settings for the specified session
     * type.
     *
     * When the dialog closes, the profile will be updated in the SessionManager
     * with the altered settings.
     *
     * @param profile The profile to be edited
     * @param state Indicates whether @p profile is an already existing profile
     * or a new one being created
     */
    void setProfile(const Profile::Ptr &profile, InitialProfileState state = EditProfileDialog::ExistingProfile);

    /**
     * Selects the text in the profile name edit area.
     * When the dialog is being used to create a new profile,
     * this can be used to draw the user's attention to the profile name
     * and make it easy for them to change it.
     */
    void selectProfileName();

public Q_SLOTS:
    // reimplemented
    void accept() override;
    // reimplemented
    void reject() override;

private Q_SLOTS:
    QSize sizeHint() const override;

    // sets up the specified tab page if necessary
    void preparePage(KPageWidgetItem *current, KPageWidgetItem *before = nullptr);

    // saves changes to profile
    void save();

    // general page
    void selectInitialDir();
    void selectIcon();

    void profileNameChanged(const QString &name);
    void initialDirChanged(const QString &dir);
    void startInSameDir(bool);
    void commandChanged(const QString &command);
    void semanticUpDown(bool);
    void semanticInputClick(bool enable);

    // tab page
    void tabTitleFormatChanged(const QString &format);
    void remoteTabTitleFormatChanged(const QString &format);
    void tabColorChanged(const QColor &color);
    void silenceSecondsChanged(int);

    void terminalColumnsEntryChanged(int);
    void terminalRowsEntryChanged(int);
    void showTerminalSizeHint(bool);
    void setDimWhenInactive(bool);
    void setDimValue(int value);
    void showEnvironmentEditor();

    // appearance page
    void setAntialiasText(bool enable);
    void setBoldIntense(bool enable);
    void useFontLineCharacters(bool enable);
    void newColorScheme();
    void editColorScheme();
    void saveColorScheme(const ColorScheme &scheme, bool isNewScheme);
    void removeColorScheme();
    void setVerticalLine(bool);
    void setVerticalLineColumn(int);
    void toggleBlinkingCursor(bool);
    void setCursorShape(int);
    void autoCursorColor();
    void customCursorColor();
    void customCursorColorChanged(const QColor &);
    void customCursorTextColorChanged(const QColor &);
    void terminalMarginChanged(int margin);
    void lineSpacingChanged(int);
    void setTerminalCenter(bool enable);
    void togglebidiRendering(bool);
    void togglebidiTableDirOverride(bool);
    void togglebidiLineLTR(bool);

#if KNEWSTUFF_VERSION >= QT_VERSION_CHECK(5, 91, 0)
    void gotNewColorSchemes(const QList<KNSCore::EntryInternal> &changedEntries);
#else
    void gotNewColorSchemes(const KNS3::Entry::List &changedEntries);
#endif

    /**
     * Deletes the selected colorscheme from the user's home dir location
     * so that the original one from the system-wide location can be used
     * instead
     */
    void resetColorScheme();

    void colorSchemeSelected();
    void previewColorScheme(const QModelIndex &index);
    void showFontDialog();
    void showEmojiFontDialog();
    void toggleMouseWheelZoom(bool enable);

    // scrolling page
    void historyModeChanged(Enum::HistoryModeEnum mode);

    void historySizeChanged(int);

    void scrollFullPage();
    void scrollHalfPage();
    void toggleHighlightScrolledLines(bool enable);
    void toggleReflowLines(bool enable);

    // keyboard page
    void editKeyBinding();
    void newKeyBinding();
    void keyBindingSelected();
    void removeKeyBinding();
    void resetKeyBindings();

    // mouse page
    void toggleUnderlineFiles(bool enable);
    void toggleUnderlineLinks(bool);
    void toggleOpenLinksByDirectClick(bool);
    void textEditorCmdEditLineChanged(const QString &text);
    void toggleCtrlRequiredForDrag(bool);
    void toggleDropUrlsAsText(bool);
    void toggleCopyTextToClipboard(bool);
    void toggleCopyTextAsHTML(bool);
    void toggleTrimLeadingSpacesInSelectedText(bool);
    void toggleTrimTrailingSpacesInSelectedText(bool);
    void pasteFromX11Selection();
    void pasteFromClipboard();
    void toggleAlternateScrolling(bool enable);
    void toggleAllowColorFilter(bool enable);
    void toggleAllowMouseTracking(bool allow);

    void TripleClickModeChanged(int);
    void wordCharactersChanged(const QString &);

    // advanced page
    void toggleBlinkingText(bool);
    void toggleFlowControl(bool);
    void updateUrlHintsModifier(bool);
    void toggleReverseUrlHints(bool);

    void setDefaultCodec(QTextCodec *);

    void setTextEditorCombo(const Profile::Ptr &profile);

    void toggleAllowLinkEscapeSequence(bool);
    void linkEscapeSequenceTextsChanged();

    void peekPrimaryKeySequenceChanged();

    void toggleWordMode(bool mode);
    void toggleWordModeAttr(bool mode);
    void toggleWordModeAscii(bool mode);
    void toggleWordModeBrahmic(bool mode);

private:
    Q_DISABLE_COPY(EditProfileDialog)

    enum PageID {
        GeneralPage = 0,
        TabsPage,
        AppearancePage,
        ScrollingPage,
        KeyboardPage,
        MousePage,
        AdvancedPage,
        PagesCount,
    };

    // initialize various pages of the dialog
    void setupGeneralPage(const Profile::Ptr &profile);
    void setupTabsPage(const Profile::Ptr &profile);
    void setupAppearancePage(const Profile::Ptr &profile);
    void setupKeyboardPage(const Profile::Ptr &profile);
    void setupScrollingPage(const Profile::Ptr &profile);
    void setupAdvancedPage(const Profile::Ptr &profile);
    void setupMousePage(const Profile::Ptr &profile);

    void setMessageGeneralPage(const QString &msg);

    int maxSpinBoxWidth(const KPluralHandlingSpinBox *spinBox, const KLocalizedString &suffix);

    // Returns the name of the colorScheme used in the current profile
    const QString currentColorSchemeName() const;

    // select @p selectedColorSchemeName after the changes are saved
    // in the colorScheme editor
    void updateColorSchemeList(const QString &selectedColorSchemeName = QString());

    void updateColorSchemeButtons();

    // Convenience method
    KeyboardTranslatorManager *_keyManager = KeyboardTranslatorManager::instance();

    // Updates the key bindings list widget on the Keyboard tab and selects
    // @p selectKeyBindingsName
    void updateKeyBindingsList(const QString &selectKeyBindingsName = QString());
    void updateKeyBindingsButtons();
    void showKeyBindingEditor(bool isNewTranslator);

    void showColorSchemeEditor(bool isNewScheme);
    void closeColorSchemeEditor();

    void preview(Profile::Property prop, const QVariant &value);
    void unpreview(Profile::Property prop);
    void unpreviewAll();
    void enableIfNonEmptySelection(QWidget *widget, QItemSelectionModel *selectionModel);

    void updateCaption(const Profile::Ptr &profile);
    void updateTransparencyWarning();

    void updateFontPreview(QFont font);
    void updateEmojiFontPreview(QFont font);

    // Update _tempProfile in a way of respecting the apply button.
    // When used with some previewed property, this method should
    // always come after the preview operation.
    void updateTempProfileProperty(Profile::Property, const QVariant &value);

    // helper method for clearing all _tempProfile properties and marking it hidden
    void resetTempProfile();

    // Enable or disable apply button, used only within
    // updateTempProfileProperty() or when toggling the default profile.
    void updateButtonApply();

    static QString groupProfileNames(const ProfileGroup::Ptr &group, int maxLength = -1);

    struct ButtonGroupOption {
        QAbstractButton *button;
        int value;
    };
    struct ButtonGroupOptions {
        QButtonGroup *group;
        Profile::Property profileProperty;
        bool preview;
        QVector<ButtonGroupOption> buttons;
    };
    void setupButtonGroup(const ButtonGroupOptions &options, const Profile::Ptr &profile);

    // returns false if:
    // - the profile name is empty
    // - the name matches the name of an already existing profile
    // - the existing profile config file is read-only
    // otherwise returns true.
    bool isProfileNameValid();

    Ui::EditProfileGeneralPage *_generalUi = nullptr;
    Ui::EditProfileTabsPage *_tabsUi = nullptr;
    Ui::EditProfileAppearancePage *_appearanceUi = nullptr;
    Ui::EditProfileScrollingPage *_scrollingUi = nullptr;
    Ui::EditProfileKeyboardPage *_keyboardUi = nullptr;
    Ui::EditProfileMousePage *_mouseUi = nullptr;
    Ui::EditProfileAdvancedPage *_advancedUi = nullptr;

    using PageSetupMethod = void (EditProfileDialog::*)(const Profile::Ptr &);
    struct Page {
        Page(PageSetupMethod page = nullptr, bool update = false)
            : setupPage(page)
            , needsUpdate(update)
        {
        }

        PageSetupMethod setupPage;
        bool needsUpdate;
    };

    QMap<KPageWidgetItem *, Page> _pages;
    KPageWidgetItem *_generalPageItem = nullptr;

    Profile::Ptr _tempProfile;
    Profile::Ptr _profile;

    bool _isDefault = false;

    Profile::PropertyMap _previewedProperties;

    ColorSchemeEditor *_colorDialog = nullptr;
    QDialogButtonBox *_buttonBox = nullptr;
    FontDialog *_fontDialog = nullptr;
    FontDialog *_emojiFontDialog = nullptr;

    InitialProfileState _profileState = ExistingProfile;
};

}

#endif // EDITPROFILEDIALOG_H
