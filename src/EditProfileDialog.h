/*
    Copyright 2007-2008 by Robert Knight <robertknight@gmail.com>
    Copyright 2018 by Harald Sitter <sitter@kde.org>

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
#include <QAbstractItemDelegate>
#include <QHash>
#include <QButtonGroup>
#include <QWidget>
#include <QGridLayout>

// KDE
#include <KPageDialog>
#include <KNS3/Entry>

// Konsole
#include "Profile.h"
#include "Enumeration.h"
#include "ColorScheme.h"
#include "ColorSchemeEditor.h"
#include "KeyboardTranslatorManager.h"
#include "FontDialog.h"

class KPluralHandlingSpinBox;
class KLocalizedString;
class QItemSelectionModel;
namespace Ui {
    class EditProfileGeneralPage;
    class EditProfileTabsPage;
    class EditProfileAppearancePage;
    class EditProfileScrollingPage;
    class EditProfileKeyboardPage;
    class EditProfileMousePage;
    class EditProfileAdvancedPage;
}

namespace Konsole {
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
class KONSOLEPRIVATE_EXPORT EditProfileDialog: public KPageDialog
{
    Q_OBJECT

public:
    /** Constructs a new dialog with the specified parent. */
    explicit EditProfileDialog(QWidget *parent = nullptr);
    ~EditProfileDialog() override;

    /**
     * Initializes the dialog with the settings for the specified session
     * type.
     *
     * When the dialog closes, the profile will be updated in the SessionManager
     * with the altered settings.
     *
     * @param profile The profile to be edited
     */
    void setProfile(const Profile::Ptr &profile);

    /**
     * Selects the text in the profile name edit area.
     * When the dialog is being used to create a new profile,
     * this can be used to draw the user's attention to the profile name
     * and make it easy for them to change it.
     */
    void selectProfileName();
    const Profile::Ptr lookupProfile() const;

public Q_SLOTS:
    // reimplemented
    void accept() override;
    // reimplemented
    void reject() override;

    void apply();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

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

    // tab page
    void tabTitleFormatChanged(const QString &format);
    void remoteTabTitleFormatChanged(const QString &format);
    void tabColorChanged(const QColor &color);
    void silenceSecondsChanged(int);

    void terminalColumnsEntryChanged(int);
    void terminalRowsEntryChanged(int);
    void showTerminalSizeHint(bool);
    void setDimWhenInactive(bool);
    void showEnvironmentEditor();

    // appearance page
    void setAntialiasText(bool enable);
    void setBoldIntense(bool enable);
    void useFontLineCharacters(bool enable);
    void newColorScheme();
    void editColorScheme();
    void saveColorScheme(const ColorScheme &scheme, bool isNewScheme);
    void removeColorScheme();
    void gotNewColorSchemes(const KNS3::Entry::List &changedEntries);

    void toggleBlinkingCursor(bool);
    void setCursorShape(int);
    void autoCursorColor();
    void customCursorColor();
    void customCursorColorChanged(const QColor &);
    void customCursorTextColorChanged(const QColor &);
    void terminalMarginChanged(int margin);
    void lineSpacingChanged(int);
    void setTerminalCenter(bool enable);

    /**
     * Deletes the selected colorscheme from the user's home dir location
     * so that the original one from the system-wide location can be used
     * instead
     */
    void resetColorScheme();

    void colorSchemeSelected();
    void previewColorScheme(const QModelIndex &index);
    void showFontDialog();
    void toggleMouseWheelZoom(bool enable);

    // scrolling page
    void historyModeChanged(Enum::HistoryModeEnum mode);

    void historySizeChanged(int);

    void scrollFullPage();
    void scrollHalfPage();
    void toggleHighlightScrolledLines(bool enable);

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
    void toggleCtrlRequiredForDrag(bool);
    void toggleDropUrlsAsText(bool);
    void toggleCopyTextToClipboard(bool);
    void toggleCopyTextAsHTML(bool);
    void toggleTrimLeadingSpacesInSelectedText(bool);
    void toggleTrimTrailingSpacesInSelectedText(bool);
    void pasteFromX11Selection();
    void pasteFromClipboard();
    void toggleAlternateScrolling(bool enable);

    void TripleClickModeChanged(int);
    void wordCharactersChanged(const QString &);

    // advanced page
    void toggleBlinkingText(bool);
    void toggleFlowControl(bool);
    void togglebidiRendering(bool);
    void updateUrlHintsModifier(bool);
    void toggleReverseUrlHints(bool);

    void setDefaultCodec(QTextCodec *);

    // apply the first previewed changes stored up by delayedPreview()
    void delayedPreviewActivate();

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
        PagesCount
    };

    // initialize various pages of the dialog
    void setupGeneralPage(const Profile::Ptr &profile);
    void setupTabsPage(const Profile::Ptr &profile);
    void setupAppearancePage(const Profile::Ptr &profile);
    void setupKeyboardPage(const Profile::Ptr &profile);
    void setupScrollingPage(const Profile::Ptr &profile);
    void setupAdvancedPage(const Profile::Ptr &profile);
    void setupMousePage(const Profile::Ptr &profile);

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

    void preview(int property, const QVariant &value);
    void delayedPreview(int property, const QVariant &value);
    void unpreview(int property);
    void unpreviewAll();
    void enableIfNonEmptySelection(QWidget *widget, QItemSelectionModel *selectionModel);

    void updateCaption(const Profile::Ptr &profile);
    void updateTransparencyWarning();

    void updateFontPreview(QFont font);

    // Update _tempProfile in a way of respecting the apply button.
    // When used with some previewed property, this method should
    // always come after the preview operation.
    void updateTempProfileProperty(Profile::Property, const QVariant &value);

    // helper method for creating an empty & hidden profile and assigning
    // it to _tempProfile.
    void createTempProfile();

    // Enable or disable apply button, used only within
    // updateTempProfileProperty().
    void updateButtonApply();

    static QString groupProfileNames(const ProfileGroup::Ptr &group, int maxLength = -1);

    struct RadioOption {
        QAbstractButton *button;
        int value;
        const char *slot;
    };
    void setupRadio(const QVector<RadioOption>& possibilities, int actual);
    struct BooleanOption {
        QAbstractButton *button;
        Profile::Property property;
        const char *slot;
    };
    void setupCheckBoxes(const QVector<BooleanOption>& options, const Profile::Ptr &profile);

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
    bool isValidProfileName();

    Ui::EditProfileGeneralPage      *_generalUi;
    Ui::EditProfileTabsPage         *_tabsUi;
    Ui::EditProfileAppearancePage   *_appearanceUi;
    Ui::EditProfileScrollingPage    *_scrollingUi;
    Ui::EditProfileKeyboardPage     *_keyboardUi;
    Ui::EditProfileMousePage        *_mouseUi;
    Ui::EditProfileAdvancedPage     *_advancedUi;

    using PageSetupMethod = void (EditProfileDialog::*)(const Profile::Ptr &);
    struct Page {
        Page(PageSetupMethod page = nullptr, bool update = false)
            : setupPage(page)
            , needsUpdate(update)
        {}

        PageSetupMethod setupPage;
        bool needsUpdate;
    };

    QMap<KPageWidgetItem *, Page> _pages;

    Profile::Ptr _tempProfile;
    Profile::Ptr _profile;

    QHash<int, QVariant> _previewedProperties;

    QHash<int, QVariant> _delayedPreviewProperties;
    QTimer *_delayedPreviewTimer;

    ColorSchemeEditor *_colorDialog;
    QDialogButtonBox *_buttonBox;
    FontDialog *_fontDialog;
};

/**
 * A delegate which can display and edit color schemes in a view.
 */
class ColorSchemeViewDelegate : public QAbstractItemDelegate
{
    Q_OBJECT

public:
    explicit ColorSchemeViewDelegate(QObject *parent = nullptr);

    // reimplemented
    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
};

/**
 * An utility class for aligning 0th column in multiple QGridLayouts.
 *
 * Limitations:
 * - a layout can't be nested in another layout
 * - reference widget must be an ancestor of all added layouts
 * - only 0th column is processed (widgets spanning multiple columns
 *   are ignored)
 */
class LabelsAligner: public QObject {
    Q_OBJECT

public:
    explicit LabelsAligner(QWidget *refWidget): _refWidget(refWidget) {}

    void addLayout(QGridLayout *layout) { _layouts.append(layout); }
    void addLayouts(const QVector<QGridLayout *> &layouts) { _layouts.append(layouts); }
    void setReferenceWidget(QWidget *refWidget) { _refWidget = refWidget; }

public Q_SLOTS:
    void updateLayouts() {
        for (const auto *layout: qAsConst(_layouts)) {
            QWidget *widget = layout->parentWidget();
            Q_ASSERT(widget);
            do {
                QLayout *widgetLayout = widget->layout();
                if (widgetLayout != nullptr) {
                    widgetLayout->update();
                    widgetLayout->activate();
                }
                widget = widget->parentWidget();
            } while (widget != _refWidget && widget != nullptr);
        }
    }

    void align() {
        Q_ASSERT(_refWidget);

        if (_layouts.count() <= 1) {
            return;
        }

        int maxRight = 0;
        for (const auto *layout: qAsConst(_layouts)) {
            int left = getLeftMargin(layout);
            for (int row = 0; row < layout->rowCount(); ++row) {
                QLayoutItem *layoutItem = layout->itemAtPosition(row, LABELS_COLUMN);
                if (layoutItem == nullptr) {
                    continue;
                }
                QWidget *widget = layoutItem->widget();
                if (widget == nullptr) {
                    continue;
                }
                const int idx = layout->indexOf(widget);
                int rows, cols, rowSpan, colSpan;
                layout->getItemPosition(idx, &rows, &cols, &rowSpan, &colSpan);
                if (colSpan > 1) {
                    continue;
                }

                const int right = left + widget->sizeHint().width();
                if (maxRight < right) {
                    maxRight = right;
                }
            }
        }

        for (auto *l: qAsConst(_layouts)) {
            int left = getLeftMargin(l);
            l->setColumnMinimumWidth(LABELS_COLUMN, maxRight - left);
        }
    }

private:
    int getLeftMargin(const QGridLayout *layout) {
        int left = layout->contentsMargins().left();

        if (layout->parent()->isWidgetType()) {
            auto *parentWidget = layout->parentWidget();
            Q_ASSERT(parentWidget);
            left += parentWidget->contentsMargins().left();
        } else {
            auto *parentLayout = qobject_cast<QLayout *>(layout->parent());
            Q_ASSERT(parentLayout);
            left += parentLayout->contentsMargins().left();
        }

        QWidget *parent = layout->parentWidget();
        while (parent != _refWidget && parent != nullptr) {
            left = parent->mapToParent(QPoint(left, 0)).x();
            parent = parent->parentWidget();
        }
        return left;
    }

    static constexpr int LABELS_COLUMN = 0;

    QWidget *_refWidget;
    QVector<QGridLayout *> _layouts;
};

}

#endif // EDITPROFILEDIALOG_H
