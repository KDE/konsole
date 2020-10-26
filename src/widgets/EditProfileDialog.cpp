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

// Own
#include "EditProfileDialog.h"

// Qt
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QIcon>
#include <QPainter>
#include <QPushButton>
#include <QStandardItem>
#include <QTextCodec>
#include <QTimer>
#include <QUrl>
#include <QStandardPaths>
#include <QRegularExpressionValidator>

// KDE
#include <KCodecAction>
#include <KIconDialog>
#include <KLocalizedString>
#include <KMessageBox>
#include <KNSCore/DownloadManager>
#include <KWindowSystem>

// Konsole
#include "ui_EditProfileGeneralPage.h"
#include "ui_EditProfileTabsPage.h"
#include "ui_EditProfileAppearancePage.h"
#include "ui_EditProfileScrollingPage.h"
#include "ui_EditProfileKeyboardPage.h"
#include "ui_EditProfileMousePage.h"
#include "ui_EditProfileAdvancedPage.h"

#include "colorscheme/ColorSchemeManager.h"

#include "keyboardtranslator/KeyboardTranslator.h"

#include "KeyBindingEditor.h"
#include "profile/ProfileManager.h"
#include "ShellCommand.h"
#include "WindowSystemInfo.h"

using namespace Konsole;

EditProfileDialog::EditProfileDialog(QWidget *parent)
    : KPageDialog(parent)
    , _generalUi(nullptr)
    , _tabsUi(nullptr)
    , _appearanceUi(nullptr)
    , _scrollingUi(nullptr)
    , _keyboardUi(nullptr)
    , _mouseUi(nullptr)
    , _advancedUi(nullptr)
    , _tempProfile(nullptr)
    , _profile(nullptr)
    , _previewedProperties(QHash<int, QVariant>())
    , _delayedPreviewProperties(QHash<int, QVariant>())
    , _delayedPreviewTimer(new QTimer(this))
    , _colorDialog(nullptr)
    , _buttonBox(nullptr)
    , _fontDialog(nullptr)
{
    setWindowTitle(i18n("Edit Profile"));
    setFaceType(KPageDialog::List);

    _buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    setButtonBox(_buttonBox);

    QPushButton *okButton = _buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    connect(_buttonBox, &QDialogButtonBox::accepted, this, &Konsole::EditProfileDialog::accept);
    connect(_buttonBox, &QDialogButtonBox::rejected, this, &Konsole::EditProfileDialog::reject);

    // disable the apply button , since no modification has been made
    _buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);

    connect(_buttonBox->button(QDialogButtonBox::Apply),
            &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::apply);

    connect(_delayedPreviewTimer, &QTimer::timeout, this,
            &Konsole::EditProfileDialog::delayedPreviewActivate);

    // Set a fallback icon for non-plasma desktops as this dialog looks
    // terrible without all the icons on the left sidebar.  On GTK related
    // desktops, this dialog look good enough without installing
    // oxygen-icon-theme, qt5ct and setting export QT_QPA_PLATFORMTHEME=qt5ct
    // Plain Xorg desktops still look terrible as there are no icons
    // visible.
    const auto defaultIcon = QIcon::fromTheme(QStringLiteral("utilities-terminal"));

    // General page

    const QString generalPageName = i18nc("@title:tab Generic, common options", "General");
    auto *generalPageWidget = new QWidget(this);
    _generalUi = new Ui::EditProfileGeneralPage();
    _generalUi->setupUi(generalPageWidget);
    auto *generalPageItem = addPage(generalPageWidget, generalPageName);
    generalPageItem->setHeader(generalPageName);
    generalPageItem->setIcon(QIcon::fromTheme(QStringLiteral("utilities-terminal")));
    _pages[generalPageItem] = Page(&EditProfileDialog::setupGeneralPage);

    // Tabs page

    const QString tabsPageName = i18n("Tabs");
    auto *tabsPageWidget = new QWidget(this);
    _tabsUi = new Ui::EditProfileTabsPage();
    _tabsUi->setupUi(tabsPageWidget);
    auto *tabsPageItem = addPage(tabsPageWidget, tabsPageName);
    tabsPageItem->setHeader(tabsPageName);
    tabsPageItem->setIcon(QIcon::fromTheme(QStringLiteral("tab-duplicate"),
                          defaultIcon));
    _pages[tabsPageItem] = Page(&EditProfileDialog::setupTabsPage);

    LabelsAligner tabsAligner(tabsPageWidget);
    tabsAligner.addLayout(dynamic_cast<QGridLayout *>(_tabsUi->tabMonitoringGroup->layout()));
    tabsAligner.addLayout(dynamic_cast<QGridLayout *>(_tabsUi->renameTabWidget->layout()));
    tabsAligner.updateLayouts();
    tabsAligner.align();

    // Appearance page

    const QString appearancePageName = i18n("Appearance");
    auto *appearancePageWidget = new QWidget(this);
    _appearanceUi = new Ui::EditProfileAppearancePage();
    _appearanceUi->setupUi(appearancePageWidget);
    auto *appearancePageItem = addPage(appearancePageWidget, appearancePageName);
    appearancePageItem->setHeader(appearancePageName);
    appearancePageItem->setIcon(QIcon::fromTheme(QStringLiteral("kcolorchooser"),
                                defaultIcon));
    _pages[appearancePageItem] = Page(&EditProfileDialog::setupAppearancePage);

    LabelsAligner appearanceAligner(appearancePageWidget);
    appearanceAligner.addLayout(dynamic_cast<QGridLayout *>(_appearanceUi->contentsGroup->layout()));
    appearanceAligner.updateLayouts();
    appearanceAligner.align();

    // Scrolling page

    const QString scrollingPageName = i18n("Scrolling");
    auto *scrollingPageWidget = new QWidget(this);
    _scrollingUi = new Ui::EditProfileScrollingPage();
    _scrollingUi->setupUi(scrollingPageWidget);
    auto *scrollingPageItem = addPage(scrollingPageWidget, scrollingPageName);
    scrollingPageItem->setHeader(scrollingPageName);
    scrollingPageItem->setIcon(QIcon::fromTheme(QStringLiteral("transform-move-vertical"),
                               defaultIcon));
    _pages[scrollingPageItem] = Page(&EditProfileDialog::setupScrollingPage);

    // adjust "history size" label height to match history size widget's first radio button
    _scrollingUi->historySizeLabel->setFixedHeight(_scrollingUi->historySizeWidget->preferredLabelHeight());

    // Keyboard page

    const QString keyboardPageName = i18n("Keyboard");
    const QString keyboardPageTitle = i18n("Key bindings");
    auto *keyboardPageWidget = new QWidget(this);
    _keyboardUi = new Ui::EditProfileKeyboardPage();
    _keyboardUi->setupUi(keyboardPageWidget);
    auto *keyboardPageItem = addPage(keyboardPageWidget, keyboardPageName);
    keyboardPageItem->setHeader(keyboardPageTitle);
    keyboardPageItem->setIcon(QIcon::fromTheme(QStringLiteral("input-keyboard"),
                              defaultIcon));
    _pages[keyboardPageItem] = Page(&EditProfileDialog::setupKeyboardPage);

    // Mouse page

    const QString mousePageName = i18n("Mouse");
    auto *mousePageWidget = new QWidget(this);
    _mouseUi = new Ui::EditProfileMousePage();
    _mouseUi->setupUi(mousePageWidget);

    const auto regExp = QRegularExpression(QStringLiteral(R"(([a-z]*:\/\/;)*([A-Za-z*]:\/\/))"));
    auto validator = new QRegularExpressionValidator(regExp, this);
    _mouseUi->linkEscapeSequenceTexts->setValidator(validator);

    auto *mousePageItem = addPage(mousePageWidget, mousePageName);
    mousePageItem->setHeader(mousePageName);
    mousePageItem->setIcon(QIcon::fromTheme(QStringLiteral("input-mouse"),
                           defaultIcon));
    _pages[mousePageItem] = Page(&EditProfileDialog::setupMousePage);

    // Advanced page

    const QString advancedPageName = i18nc("@title:tab Complex options", "Advanced");
    auto *advancedPageWidget = new QWidget(this);
    _advancedUi = new Ui::EditProfileAdvancedPage();
    _advancedUi->setupUi(advancedPageWidget);
    auto *advancedPageItem = addPage(advancedPageWidget, advancedPageName);
    advancedPageItem->setHeader(advancedPageName);
    advancedPageItem->setIcon(QIcon::fromTheme(QStringLiteral("configure"),
                              defaultIcon));
    _pages[advancedPageItem] = Page(&EditProfileDialog::setupAdvancedPage);

    // there are various setupXYZPage() methods to load the items
    // for each page and update their states to match the profile
    // being edited.
    //
    // these are only called when needed ( ie. when the user clicks
    // the tab to move to that page ).
    //
    // the _pageNeedsUpdate vector keeps track of the pages that have
    // not been updated since the last profile change and will need
    // to be refreshed when the user switches to them
    connect(this, &KPageDialog::currentPageChanged,
            this, &Konsole::EditProfileDialog::preparePage);

    createTempProfile();
}

EditProfileDialog::~EditProfileDialog()
{
    delete _generalUi;
    delete _tabsUi;
    delete _appearanceUi;
    delete _scrollingUi;
    delete _keyboardUi;
    delete _mouseUi;
    delete _advancedUi;
}

void EditProfileDialog::save()
{
    if (_tempProfile->isEmpty()) {
        return;
    }

    const bool isFallback = _profile->path() == QLatin1String("FALLBACK/");

    ProfileManager::instance()->changeProfile(_profile, _tempProfile->setProperties());

    // ensure that these settings are not undone by a call
    // to unpreview()
    QHashIterator<Profile::Property, QVariant> iter(_tempProfile->setProperties());
    while (iter.hasNext()) {
        iter.next();
        _previewedProperties.remove(iter.key());
    }

    createTempProfile();

    if (isFallback) {
        // Needed to update the profile name in the dialog, if the user
        // used "Apply" and the dialog is still open, since the fallback
        // profile will have a unique name, e.g. "Profile 1", generated
        // for it by the ProfileManager.
        setProfile(_profile);
    }

    _buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void EditProfileDialog::reject()
{
    unpreviewAll();
    QDialog::reject();
}

void EditProfileDialog::accept()
{
    // if the Apply button is disabled then no settings were changed
    // or the changes have already been saved by apply()
    if (_buttonBox->button(QDialogButtonBox::Apply)->isEnabled()) {
        if (!isValidProfileName()) {
            return;
        }
        save();
    }

    unpreviewAll();
    QDialog::accept();
}

void EditProfileDialog::apply()
{
    if (!isValidProfileName()) {
        return;
    }
    save();
}

bool EditProfileDialog::isValidProfileName()
{
    Q_ASSERT(_profile);
    Q_ASSERT(_tempProfile);

    // check whether the user has enough permissions to save the profile
    QFileInfo fileInfo(_profile->path());
    if (fileInfo.exists() && !fileInfo.isWritable()) {
        if (!_tempProfile->isPropertySet(Profile::Name)
            || (_tempProfile->name() == _profile->name())) {
                KMessageBox::sorry(this,
                                   i18n("<p>Konsole does not have permission to save this profile to:<br /> \"%1\"</p>"
                                        "<p>To be able to save settings you can either change the permissions "
                                        "of the profile configuration file or change the profile name to save "
                                        "the settings to a new profile.</p>", _profile->path()));
                return false;
        }
    }

    const QList<Profile::Ptr> existingProfiles = ProfileManager::instance()->allProfiles();
    QStringList otherExistingProfileNames;

    for (const Profile::Ptr &existingProfile : existingProfiles) {
        if (existingProfile->name() != _profile->name()) {
            otherExistingProfileNames.append(existingProfile->name());
        }
    }

    if ((_tempProfile->isPropertySet(Profile::Name)
         && _tempProfile->name().isEmpty())
        || (_profile->name().isEmpty() && _tempProfile->name().isEmpty())) {
        KMessageBox::sorry(this,
                           i18n("<p>Each profile must have a name before it can be saved "
                                "into disk.</p>"));
        // revert the name in the dialog
        _generalUi->profileNameEdit->setText(_profile->name());
        selectProfileName();
        return false;
    } else if (!_tempProfile->name().isEmpty() && otherExistingProfileNames.contains(_tempProfile->name())) {
        KMessageBox::sorry(this,
                            i18n("<p>A profile with this name already exists.</p>"));
        // revert the name in the dialog
        _generalUi->profileNameEdit->setText(_profile->name());
        selectProfileName();
        return false;
    } else {
        return true;
    }
}

QString EditProfileDialog::groupProfileNames(const ProfileGroup::Ptr &group, int maxLength)
{
    QString caption;
    int count = group->profiles().count();
    for (int i = 0; i < count; i++) {
        caption += group->profiles()[i]->name();
        if (i < (count - 1)) {
            caption += QLatin1Char(',');
            // limit caption length to prevent very long window titles
            if (maxLength > 0 && caption.length() > maxLength) {
                caption += QLatin1String("...");
                break;
            }
        }
    }
    return caption;
}

void EditProfileDialog::updateCaption(const Profile::Ptr &profile)
{
    const int MAX_GROUP_CAPTION_LENGTH = 25;
    ProfileGroup::Ptr group = profile->asGroup();
    if (group && group->profiles().count() > 1) {
        QString caption = groupProfileNames(group, MAX_GROUP_CAPTION_LENGTH);
        setWindowTitle(i18np("Editing profile: %2",
                             "Editing %1 profiles: %2",
                             group->profiles().count(),
                             caption));
    } else {
        setWindowTitle(i18n("Edit Profile \"%1\"", profile->name()));
    }
}

void EditProfileDialog::setProfile(const Konsole::Profile::Ptr &profile)
{
    Q_ASSERT(profile);

    _profile = profile;

    // update caption
    updateCaption(profile);

    // mark each page of the dialog as out of date
    // and force an update of the currently visible page
    //
    // the other pages will be updated as necessary
    for (Page &page: _pages) {
        page.needsUpdate = true;
    }
    preparePage(currentPage());

    if (_tempProfile) {
        createTempProfile();
    }
}

const Profile::Ptr EditProfileDialog::lookupProfile() const
{
    return _profile;
}

const QString EditProfileDialog::currentColorSchemeName() const
{
    const QString &currentColorSchemeName = lookupProfile()->colorScheme();
    return currentColorSchemeName;
}

void EditProfileDialog::preparePage(KPageWidgetItem *current, KPageWidgetItem *before)
{
    Q_UNUSED(before)
    Q_ASSERT(current);
    Q_ASSERT(_pages.contains(current));

    const Profile::Ptr profile = lookupProfile();
    auto setupPage = _pages[current].setupPage;
    Q_ASSERT(profile);
    Q_ASSERT(setupPage);

    if (_pages[current].needsUpdate) {
        (*this.*setupPage)(profile);
        _pages[current].needsUpdate = false;
    }
}

void Konsole::EditProfileDialog::selectProfileName()
{
    _generalUi->profileNameEdit->setFocus();
    _generalUi->profileNameEdit->selectAll();
}

void EditProfileDialog::setupGeneralPage(const Profile::Ptr &profile)
{
    // basic profile options
    {
        ProfileGroup::Ptr group = profile->asGroup();
        if (!group || group->profiles().count() < 2) {
            _generalUi->profileNameEdit->setText(profile->name());
            _generalUi->profileNameEdit->setClearButtonEnabled(true);
        } else {
            _generalUi->profileNameEdit->setText(groupProfileNames(group, -1));
            _generalUi->profileNameEdit->setEnabled(false);
        }
    }

    ShellCommand command(profile->command(), profile->arguments());
    _generalUi->commandEdit->setText(command.fullCommand());
    // If a "completion" is requested, consider changing this to KLineEdit
    // and using KCompletion.
    _generalUi->initialDirEdit->setText(profile->defaultWorkingDirectory());
    _generalUi->initialDirEdit->setClearButtonEnabled(true);
    _generalUi->initialDirEdit->setPlaceholderText(QStandardPaths::standardLocations(QStandardPaths::HomeLocation).value(0));

    _generalUi->dirSelectButton->setIcon(QIcon::fromTheme(QStringLiteral("folder-open")));
    _generalUi->iconSelectButton->setIcon(QIcon::fromTheme(profile->icon()));
    _generalUi->startInSameDirButton->setChecked(profile->startInCurrentSessionDir());

    // initial terminal size
    const auto colsSuffix = ki18ncp("Suffix of the number of columns (N columns). The leading space is needed to separate it from the number value.", " column", " columns");
    const auto rowsSuffix = ki18ncp("Suffix of the number of rows (N rows). The leading space is needed to separate it from the number value.", " row", " rows");
    _generalUi->terminalColumnsEntry->setValue(profile->terminalColumns());
    _generalUi->terminalRowsEntry->setValue(profile->terminalRows());
    _generalUi->terminalColumnsEntry->setSuffix(colsSuffix);
    _generalUi->terminalRowsEntry->setSuffix(rowsSuffix);
    // make width of initial terminal size spinboxes equal
    const int sizeEntryWidth = qMax(maxSpinBoxWidth(_generalUi->terminalColumnsEntry, colsSuffix),
                                    maxSpinBoxWidth(_generalUi->terminalRowsEntry, rowsSuffix));
    _generalUi->terminalColumnsEntry->setFixedWidth(sizeEntryWidth);
    _generalUi->terminalRowsEntry->setFixedWidth(sizeEntryWidth);

    // signals and slots
    connect(_generalUi->dirSelectButton, &QToolButton::clicked, this,
            &Konsole::EditProfileDialog::selectInitialDir);
    connect(_generalUi->iconSelectButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::selectIcon);
    connect(_generalUi->startInSameDirButton, &QCheckBox::toggled, this,
            &Konsole::EditProfileDialog::startInSameDir);
    connect(_generalUi->profileNameEdit, &QLineEdit::textChanged, this,
            &Konsole::EditProfileDialog::profileNameChanged);
    connect(_generalUi->initialDirEdit, &QLineEdit::textChanged, this,
            &Konsole::EditProfileDialog::initialDirChanged);
    connect(_generalUi->commandEdit, &QLineEdit::textChanged, this,
            &Konsole::EditProfileDialog::commandChanged);
    connect(_generalUi->environmentEditButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::showEnvironmentEditor);

    connect(_generalUi->terminalColumnsEntry,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Konsole::EditProfileDialog::terminalColumnsEntryChanged);
    connect(_generalUi->terminalRowsEntry,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Konsole::EditProfileDialog::terminalRowsEntryChanged);
}

void EditProfileDialog::showEnvironmentEditor()
{
    bool ok;
    const Profile::Ptr profile = lookupProfile();

    QStringList currentEnvironment;

    // The user could re-open the environment editor before clicking
    // OK/Apply in the parent edit profile dialog, so we make sure
    // to show the new environment vars
    if (_tempProfile->isPropertySet(Profile::Environment)) {
            currentEnvironment  = _tempProfile->environment();
    } else {
        currentEnvironment = profile->environment();
    }

    QString text = QInputDialog::getMultiLineText(this,
                                                  i18n("Edit Environment"),
                                                  i18n("One environment variable per line"),
                                                  currentEnvironment.join(QStringLiteral("\n")),
                                                  &ok);

    QStringList newEnvironment;

    if (ok) {
        if(!text.isEmpty()) {
            newEnvironment = text.split(QLatin1Char('\n'));
            updateTempProfileProperty(Profile::Environment, newEnvironment);
        } else {
            // the user could have removed all entries so we return an empty list
            updateTempProfileProperty(Profile::Environment, newEnvironment);
        }
    }
}

void EditProfileDialog::setupTabsPage(const Profile::Ptr &profile)
{
    // tab title format
    _tabsUi->renameTabWidget->setTabTitleText(profile->localTabTitleFormat());
    _tabsUi->renameTabWidget->setRemoteTabTitleText(profile->remoteTabTitleFormat());
    _tabsUi->renameTabWidget->setColor(profile->tabColor());

    connect(_tabsUi->renameTabWidget, &Konsole::RenameTabWidget::tabTitleFormatChanged, this,
            &Konsole::EditProfileDialog::tabTitleFormatChanged);
    connect(_tabsUi->renameTabWidget, &Konsole::RenameTabWidget::remoteTabTitleFormatChanged, this,
            &Konsole::EditProfileDialog::remoteTabTitleFormatChanged);
    connect(_tabsUi->renameTabWidget, &Konsole::RenameTabWidget::tabColorChanged, this,
            &Konsole::EditProfileDialog::tabColorChanged);

    // tab monitoring
    const int silenceSeconds = profile->silenceSeconds();
    _tabsUi->silenceSecondsSpinner->setValue(silenceSeconds);
    auto suffix = ki18ncp("Unit of time", " second", " seconds");
    _tabsUi->silenceSecondsSpinner->setSuffix(suffix);
    int silenceCheckBoxWidth = maxSpinBoxWidth(_generalUi->terminalColumnsEntry, suffix);
    _tabsUi->silenceSecondsSpinner->setFixedWidth(silenceCheckBoxWidth);

    connect(_tabsUi->silenceSecondsSpinner,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Konsole::EditProfileDialog::silenceSecondsChanged);
}

void EditProfileDialog::terminalColumnsEntryChanged(int value)
{
    updateTempProfileProperty(Profile::TerminalColumns, value);
}

void EditProfileDialog::terminalRowsEntryChanged(int value)
{
    updateTempProfileProperty(Profile::TerminalRows, value);
}

void EditProfileDialog::showTerminalSizeHint(bool value)
{
    updateTempProfileProperty(Profile::ShowTerminalSizeHint, value);
}

void EditProfileDialog::setDimWhenInactive(bool value)
{
    updateTempProfileProperty(Profile::DimWhenInactive, value);
}

void EditProfileDialog::setDimValue(int value)
{
    updateTempProfileProperty(Profile::DimValue, value);
}

void EditProfileDialog::tabTitleFormatChanged(const QString &format)
{
    updateTempProfileProperty(Profile::LocalTabTitleFormat, format);
}

void EditProfileDialog::remoteTabTitleFormatChanged(const QString &format)
{
    updateTempProfileProperty(Profile::RemoteTabTitleFormat, format);
}

void EditProfileDialog::tabColorChanged(const QColor &color)
{
    updateTempProfileProperty(Profile::TabColor, color);
}

void EditProfileDialog::silenceSecondsChanged(int seconds)
{
    updateTempProfileProperty(Profile::SilenceSeconds, seconds);
}

void EditProfileDialog::selectIcon()
{
    const QString &icon = KIconDialog::getIcon(KIconLoader::Desktop, KIconLoader::Application,
                                               false, 0, false, this);
    if (!icon.isEmpty()) {
        _generalUi->iconSelectButton->setIcon(QIcon::fromTheme(icon));
        updateTempProfileProperty(Profile::Icon, icon);
    }
}

void EditProfileDialog::profileNameChanged(const QString &name)
{
    updateTempProfileProperty(Profile::Name, name);
    updateTempProfileProperty(Profile::UntranslatedName, name);
    updateCaption(_tempProfile);
}

void EditProfileDialog::startInSameDir(bool sameDir)
{
    updateTempProfileProperty(Profile::StartInCurrentSessionDir, sameDir);
}

void EditProfileDialog::initialDirChanged(const QString &dir)
{
    updateTempProfileProperty(Profile::Directory, dir);
}

void EditProfileDialog::commandChanged(const QString &command)
{
    ShellCommand shellCommand(command);

    updateTempProfileProperty(Profile::Command, shellCommand.command());
    updateTempProfileProperty(Profile::Arguments, shellCommand.arguments());
}

void EditProfileDialog::selectInitialDir()
{
    const QUrl url = QFileDialog::getExistingDirectoryUrl(this,
                                                          i18n("Select Initial Directory"),
                                                          QUrl::fromUserInput(_generalUi->initialDirEdit->text()));

    if (!url.isEmpty()) {
        _generalUi->initialDirEdit->setText(url.path());
    }
}

void EditProfileDialog::setupAppearancePage(const Profile::Ptr &profile)
{
    auto delegate = new ColorSchemeViewDelegate(this);
    _appearanceUi->colorSchemeList->setItemDelegate(delegate);

    _appearanceUi->transparencyWarningWidget->setVisible(false);
    _appearanceUi->transparencyWarningWidget->setWordWrap(true);
    _appearanceUi->transparencyWarningWidget->setCloseButtonVisible(false);
    _appearanceUi->transparencyWarningWidget->setMessageType(KMessageWidget::Warning);

    _appearanceUi->colorSchemeMessageWidget->setVisible(false);
    _appearanceUi->colorSchemeMessageWidget->setWordWrap(true);
    _appearanceUi->colorSchemeMessageWidget->setCloseButtonVisible(false);
    _appearanceUi->colorSchemeMessageWidget->setMessageType(KMessageWidget::Warning);

    _appearanceUi->editColorSchemeButton->setEnabled(false);
    _appearanceUi->removeColorSchemeButton->setEnabled(false);
    _appearanceUi->resetColorSchemeButton->setEnabled(false);

    _appearanceUi->downloadColorSchemeButton->setConfigFile(QStringLiteral("konsole.knsrc"));

    // setup color list
    // select the colorScheme used in the current profile
    updateColorSchemeList(currentColorSchemeName());

    _appearanceUi->colorSchemeList->setMouseTracking(true);
    _appearanceUi->colorSchemeList->installEventFilter(this);
    _appearanceUi->colorSchemeList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    connect(_appearanceUi->colorSchemeList->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            &Konsole::EditProfileDialog::colorSchemeSelected);
    connect(_appearanceUi->colorSchemeList, &QListView::entered, this,
            &Konsole::EditProfileDialog::previewColorScheme);

    updateColorSchemeButtons();

    connect(_appearanceUi->editColorSchemeButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::editColorScheme);
    connect(_appearanceUi->removeColorSchemeButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::removeColorScheme);
    connect(_appearanceUi->newColorSchemeButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::newColorScheme);
    connect(_appearanceUi->downloadColorSchemeButton, &KNS3::Button::dialogFinished, this,
            &Konsole::EditProfileDialog::gotNewColorSchemes);

    connect(_appearanceUi->resetColorSchemeButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::resetColorScheme);

    connect(_appearanceUi->chooseFontButton, &QAbstractButton::clicked, this,
            &EditProfileDialog::showFontDialog);

    // setup font preview
    const bool antialias = profile->antiAliasFonts();

    QFont profileFont = profile->font();
    profileFont.setStyleStrategy(antialias ? QFont::PreferAntialias : QFont::NoAntialias);

    _appearanceUi->fontPreview->setFont(profileFont);
    _appearanceUi->fontPreview->setText(QStringLiteral("%1 %2pt").arg(profileFont.family()).arg(profileFont.pointSize()));

    // setup font smoothing
    _appearanceUi->antialiasTextButton->setChecked(antialias);
    connect(_appearanceUi->antialiasTextButton, &QCheckBox::toggled, this,
            &Konsole::EditProfileDialog::setAntialiasText);

    _appearanceUi->boldIntenseButton->setChecked(profile->boldIntense());
    connect(_appearanceUi->boldIntenseButton, &QCheckBox::toggled, this,
            &Konsole::EditProfileDialog::setBoldIntense);

    _appearanceUi->useFontLineCharactersButton->setChecked(profile->useFontLineCharacters());
    connect(_appearanceUi->useFontLineCharactersButton, &QCheckBox::toggled, this,
            &Konsole::EditProfileDialog::useFontLineCharacters);

    _mouseUi->enableMouseWheelZoomButton->setChecked(profile->mouseWheelZoomEnabled());
    connect(_mouseUi->enableMouseWheelZoomButton, &QCheckBox::toggled, this,
            &Konsole::EditProfileDialog::toggleMouseWheelZoom);

    // cursor options
    _appearanceUi->enableBlinkingCursorButton->setChecked(profile->property<bool>(Profile::BlinkingCursorEnabled));
    connect(_appearanceUi->enableBlinkingCursorButton, &QToolButton::toggled, this, &EditProfileDialog::toggleBlinkingCursor);

    if (profile->useCustomCursorColor()) {
        _appearanceUi->customCursorColorButton->setChecked(true);
    } else {
        _appearanceUi->autoCursorColorButton->setChecked(true);
    }

    _appearanceUi->customColorSelectButton->setColor(profile->customCursorColor());
    _appearanceUi->customTextColorSelectButton->setColor(profile->customCursorTextColor());

    connect(_appearanceUi->customCursorColorButton, &QRadioButton::clicked, this, &Konsole::EditProfileDialog::customCursorColor);
    connect(_appearanceUi->autoCursorColorButton, &QRadioButton::clicked, this, &Konsole::EditProfileDialog::autoCursorColor);
    connect(_appearanceUi->customColorSelectButton, &KColorButton::changed, this, &Konsole::EditProfileDialog::customCursorColorChanged);
    connect(_appearanceUi->customTextColorSelectButton, &KColorButton::changed, this, &Konsole::EditProfileDialog::customCursorTextColorChanged);

    const ButtonGroupOptions cursorShapeOptions = {
        _appearanceUi->cursorShape, // group
        Profile::CursorShape,       // profileProperty
        true,                       // preview
        {                           // buttons
            {_appearanceUi->cursorShapeBlock,       Enum::BlockCursor},
            {_appearanceUi->cursorShapeIBeam,       Enum::IBeamCursor},
            {_appearanceUi->cursorShapeUnderline,   Enum::UnderlineCursor},
        },
    };
    setupButtonGroup(cursorShapeOptions, profile);

    _appearanceUi->marginsSpinner->setValue(profile->terminalMargin());
    connect(_appearanceUi->marginsSpinner,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Konsole::EditProfileDialog::terminalMarginChanged);

    _appearanceUi->lineSpacingSpinner->setValue(profile->lineSpacing());
    connect(_appearanceUi->lineSpacingSpinner,
            QOverload<int>::of(&QSpinBox::valueChanged), this,
            &Konsole::EditProfileDialog::lineSpacingChanged);

    _appearanceUi->alignToCenterButton->setChecked(profile->terminalCenter());
    connect(_appearanceUi->alignToCenterButton, &QCheckBox::toggled, this,
            &Konsole::EditProfileDialog::setTerminalCenter);

    _appearanceUi->showTerminalSizeHintButton->setChecked(profile->showTerminalSizeHint());
    connect(_appearanceUi->showTerminalSizeHintButton, &QCheckBox::toggled, this,
            &Konsole::EditProfileDialog::showTerminalSizeHint);

    _appearanceUi->dimWhenInactiveCheckbox->setChecked(profile->dimWhenInactive());
    connect(_appearanceUi->dimWhenInactiveCheckbox, &QCheckBox::toggled, this,
            &Konsole::EditProfileDialog::setDimWhenInactive);

    _appearanceUi->dimValue->setValue(profile->dimValue());
    _appearanceUi->dimValue->setEnabled(profile->dimWhenInactive());
    _appearanceUi->dimLabel->setEnabled(profile->dimWhenInactive());
    connect(_appearanceUi->dimValue, &QSlider::valueChanged,
            this, &Konsole::EditProfileDialog::setDimValue);

    _appearanceUi->displayVerticalLine->setChecked(profile->verticalLine());
    connect(_appearanceUi->displayVerticalLine, &QCheckBox::toggled,
            this, &EditProfileDialog::setVerticalLine);

    _appearanceUi->displayVerticalLineAtColumn->setValue(profile->verticalLineAtChar());
    connect(_appearanceUi->displayVerticalLineAtColumn, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &EditProfileDialog::setVerticalLineColumn);
}

void EditProfileDialog::setAntialiasText(bool enable)
{
    preview(Profile::AntiAliasFonts, enable);
    updateTempProfileProperty(Profile::AntiAliasFonts, enable);

    const QFont font = _profile->font();
    updateFontPreview(font);
}

void EditProfileDialog::toggleAllowLinkEscapeSequence(bool enable)
{
    updateTempProfileProperty(Profile::AllowEscapedLinks, enable);
}

void EditProfileDialog::linkEscapeSequenceTextsChanged()
{
    updateTempProfileProperty(Profile::EscapedLinksSchema, _mouseUi->linkEscapeSequenceTexts->text());
}
void EditProfileDialog::setVerticalLine(bool value)
{
    updateTempProfileProperty(Profile::VerticalLine, value);
}

void EditProfileDialog::setVerticalLineColumn(int value)
{
    updateTempProfileProperty(Profile::VerticalLineAtChar, value);
}

void EditProfileDialog::setBoldIntense(bool enable)
{
    preview(Profile::BoldIntense, enable);
    updateTempProfileProperty(Profile::BoldIntense, enable);
}

void EditProfileDialog::useFontLineCharacters(bool enable)
{
    preview(Profile::UseFontLineCharacters, enable);
    updateTempProfileProperty(Profile::UseFontLineCharacters, enable);
}

void EditProfileDialog::toggleBlinkingCursor(bool enable)
{
    preview(Profile::BlinkingCursorEnabled, enable);
    updateTempProfileProperty(Profile::BlinkingCursorEnabled, enable);
}

void EditProfileDialog::setCursorShape(int index)
{
    preview(Profile::CursorShape, index);
    updateTempProfileProperty(Profile::CursorShape, index);
}

void EditProfileDialog::autoCursorColor()
{
    preview(Profile::UseCustomCursorColor, false);
    updateTempProfileProperty(Profile::UseCustomCursorColor, false);
}

void EditProfileDialog::customCursorColor()
{
    preview(Profile::UseCustomCursorColor, true);
    updateTempProfileProperty(Profile::UseCustomCursorColor, true);
}

void EditProfileDialog::customCursorColorChanged(const QColor &color)
{
    preview(Profile::CustomCursorColor, color);
    updateTempProfileProperty(Profile::CustomCursorColor, color);

    // ensure that custom cursor colors are enabled
    _appearanceUi->customCursorColorButton->click();
}

void EditProfileDialog::customCursorTextColorChanged(const QColor &color)
{
    preview(Profile::CustomCursorTextColor, color);
    updateTempProfileProperty(Profile::CustomCursorTextColor, color);

    // ensure that custom cursor colors are enabled
    _appearanceUi->customCursorColorButton->click();
}

void EditProfileDialog::terminalMarginChanged(int margin)
{
    preview(Profile::TerminalMargin, margin);
    updateTempProfileProperty(Profile::TerminalMargin, margin);
}

void EditProfileDialog::lineSpacingChanged(int spacing)
{
    preview(Profile::LineSpacing, spacing);
    updateTempProfileProperty(Profile::LineSpacing, spacing);
}

void EditProfileDialog::setTerminalCenter(bool enable)
{
    preview(Profile::TerminalCenter, enable);
    updateTempProfileProperty(Profile::TerminalCenter, enable);
}

void EditProfileDialog::toggleMouseWheelZoom(bool enable)
{
    updateTempProfileProperty(Profile::MouseWheelZoomEnabled, enable);
}

void EditProfileDialog::toggleAlternateScrolling(bool enable)
{
    updateTempProfileProperty(Profile::AlternateScrolling, enable);
}

void EditProfileDialog::updateColorSchemeList(const QString &selectedColorSchemeName)
{
    if (_appearanceUi->colorSchemeList->model() == nullptr) {
        _appearanceUi->colorSchemeList->setModel(new QStandardItemModel(this));
    }

    const ColorScheme *selectedColorScheme = ColorSchemeManager::instance()->findColorScheme(selectedColorSchemeName);

    auto *model = qobject_cast<QStandardItemModel *>(_appearanceUi->colorSchemeList->model());

    Q_ASSERT(model);

    model->clear();

    QStandardItem *selectedItem = nullptr;

   const QList<const ColorScheme *> schemeList = ColorSchemeManager::instance()->allColorSchemes();

    for (const ColorScheme *scheme : schemeList) {
        QStandardItem *item = new QStandardItem(scheme->description());
        item->setData(QVariant::fromValue(scheme), Qt::UserRole + 1);
        item->setData(QVariant::fromValue(_profile->font()), Qt::UserRole + 2);
        item->setFlags(item->flags());

        // if selectedColorSchemeName is not empty then select that scheme
        // after saving the changes in the colorScheme editor
        if (selectedColorScheme == scheme) {
            selectedItem = item;
        }

        model->appendRow(item);
    }

    model->sort(0);

    if (selectedItem != nullptr) {
        _appearanceUi->colorSchemeList->updateGeometry();
        _appearanceUi->colorSchemeList->selectionModel()->setCurrentIndex(selectedItem->index(),
                                                                QItemSelectionModel::Select);

        // update transparency warning label
        updateTransparencyWarning();
    }
}

void EditProfileDialog::updateKeyBindingsList(const QString &selectKeyBindingsName)
{
    if (_keyboardUi->keyBindingList->model() == nullptr) {
        _keyboardUi->keyBindingList->setModel(new QStandardItemModel(this));
    }

    auto *model = qobject_cast<QStandardItemModel *>(_keyboardUi->keyBindingList->model());

    Q_ASSERT(model);

    model->clear();

    QStandardItem *selectedItem = nullptr;

    const QStringList &translatorNames = _keyManager->allTranslators();
    for (const QString &translatorName : translatorNames) {
        const KeyboardTranslator *translator = _keyManager->findTranslator(translatorName);
        if (translator == nullptr) {
            continue;
        }

        QStandardItem *item = new QStandardItem(translator->description());
        item->setEditable(false);
        item->setData(QVariant::fromValue(translator), Qt::UserRole + 1);
        item->setData(QVariant::fromValue(_keyManager->findTranslatorPath(translatorName)), Qt::ToolTipRole);
        item->setData(QVariant::fromValue(_profile->font()), Qt::UserRole + 2);
        item->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-keyboard")));

        if (selectKeyBindingsName == translatorName) {
            selectedItem = item;
        }

        model->appendRow(item);
    }

    model->sort(0);

    if (selectedItem != nullptr) {
        _keyboardUi->keyBindingList->selectionModel()->setCurrentIndex(selectedItem->index(),
                                                               QItemSelectionModel::Select);
    }
}

bool EditProfileDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == _appearanceUi->colorSchemeList && event->type() == QEvent::Leave) {
        if (_tempProfile->isPropertySet(Profile::ColorScheme)) {
            preview(Profile::ColorScheme, _tempProfile->colorScheme());
        } else {
            unpreview(Profile::ColorScheme);
        }
    }

    return QDialog::eventFilter(watched, event);
}

QSize EditProfileDialog::sizeHint() const
{
    QFontMetrics fm(font());
    const int ch = fm.boundingRect(QLatin1Char('0')).width();

    // By default minimum size is used. Increase it to make text inputs
    // on "tabs" page wider and to add some whitespace on right side
    // of other pages. The window will not be wider than 2/3 of
    // the screen width (unless necessary to fit everything)
    return QDialog::sizeHint() + QSize(10*ch, 0);
}

void EditProfileDialog::unpreviewAll()
{
    _delayedPreviewTimer->stop();
    _delayedPreviewProperties.clear();

    QHash<Profile::Property, QVariant> map;
    QHashIterator<int, QVariant> iter(_previewedProperties);
    while (iter.hasNext()) {
        iter.next();
        map.insert(static_cast<Profile::Property>(iter.key()), iter.value());
    }

    // undo any preview changes
    if (!map.isEmpty()) {
        ProfileManager::instance()->changeProfile(_profile, map, false);
    }
}

void EditProfileDialog::unpreview(int property)
{
    _delayedPreviewProperties.remove(property);

    if (!_previewedProperties.contains(property)) {
        return;
    }

    QHash<Profile::Property, QVariant> map;
    map.insert(static_cast<Profile::Property>(property), _previewedProperties[property]);
    ProfileManager::instance()->changeProfile(_profile, map, false);

    _previewedProperties.remove(property);
}

void EditProfileDialog::delayedPreview(int property, const QVariant &value)
{
    _delayedPreviewProperties.insert(property, value);

    _delayedPreviewTimer->stop();
    _delayedPreviewTimer->start(300);
}

void EditProfileDialog::delayedPreviewActivate()
{
    Q_ASSERT(qobject_cast<QTimer *>(sender()));

    QMutableHashIterator<int, QVariant> iter(_delayedPreviewProperties);
    if (iter.hasNext()) {
        iter.next();
        preview(iter.key(), iter.value());
    }
}

void EditProfileDialog::preview(int property, const QVariant &value)
{
    QHash<Profile::Property, QVariant> map;
    map.insert(static_cast<Profile::Property>(property), value);

    _delayedPreviewProperties.remove(property);

    const Profile::Ptr original = lookupProfile();

    // skip previews for profile groups if the profiles in the group
    // have conflicting original values for the property
    //
    // TODO - Save the original values for each profile and use to unpreview properties
    ProfileGroup::Ptr group = original->asGroup();
    if (group && group->profiles().count() > 1
        && original->property<QVariant>(static_cast<Profile::Property>(property)).isNull()) {
        return;
    }

    if (!_previewedProperties.contains(property)) {
        _previewedProperties.insert(property,
                                    original->property<QVariant>(static_cast<Profile::Property>(property)));
    }

    // temporary change to color scheme
    ProfileManager::instance()->changeProfile(_profile, map, false);
}

void EditProfileDialog::previewColorScheme(const QModelIndex &index)
{
    const QString &name = index.data(Qt::UserRole + 1).value<const ColorScheme *>()->name();
    delayedPreview(Profile::ColorScheme, name);
}

void EditProfileDialog::showFontDialog()
{
    if (_fontDialog == nullptr) {
        _fontDialog = new FontDialog(this);
        connect(_fontDialog, &FontDialog::fontChanged, this, [this](const QFont &font) {
            preview(Profile::Font, font);
            updateFontPreview(font);
        });
        connect(_fontDialog, &FontDialog::accepted, this, [this]() {
            const QFont font = _fontDialog->font();
            preview(Profile::Font, font);
            updateTempProfileProperty(Profile::Font, font);
            updateFontPreview(font);
        });
        connect(_fontDialog, &FontDialog::rejected, this, [this]() {
            unpreview(Profile::Font);
            const QFont font = _profile->font();
            updateFontPreview(font);
        });
    }
    const QFont font = _profile->font();
    _fontDialog->setFont(font);
    _fontDialog->exec();
}

void EditProfileDialog::updateFontPreview(QFont font)
{
    bool aa = _profile->antiAliasFonts();
    font.setStyleStrategy(aa ? QFont::PreferAntialias : QFont::NoAntialias);

    _appearanceUi->fontPreview->setFont(font);
    _appearanceUi->fontPreview->setText(QStringLiteral("%1 %2pt").arg(font.family()).arg(font.pointSize()));
}

void EditProfileDialog::removeColorScheme()
{
    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();
    if (selected.isEmpty()) {
        return;
    }

    // The actual delete runs async because we need to on-demand query
    // files managed by KNS. Deleting files managed by KNS screws up the
    // KNS states (entry gets shown as installed when in fact we deleted it).
    auto *manager = new KNSCore::DownloadManager(QStringLiteral("konsole.knsrc"), this);
    connect(manager, &KNSCore::DownloadManager::searchResult,
            this, [=](const KNSCore::EntryInternal::List &entries) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const ColorScheme *>()->name();
        Q_ASSERT(!name.isEmpty());
        bool uninstalled = false;
        // Check if the theme was installed by KNS, if so uninstall it through
        // there and unload it.
        for (auto &entry : entries) {
            for (const auto &file : entry.installedFiles()) {
                if (ColorSchemeManager::colorSchemeNameFromPath(file) != name) {
                    continue;
                }
                // Make sure the manager can unload it before uninstalling it.
                if (ColorSchemeManager::instance()->unloadColorScheme(file)) {
                    manager->uninstallEntry(entry);
                    uninstalled = true;
                }
            }
            if (uninstalled) {
                break;
            }
        }

        // If KNS wasn't able to remove it is a custom theme and we'll drop
        // it manually.
        if (!uninstalled) {
            uninstalled = ColorSchemeManager::instance()->deleteColorScheme(name);
        }

        if (uninstalled) {
            _appearanceUi->colorSchemeList->model()->removeRow(selected.first().row());
        }

        manager->deleteLater();
    });
    manager->checkForInstalled();
}

void EditProfileDialog::gotNewColorSchemes(const KNS3::Entry::List &changedEntries)
{
    int failures = 0;
    for (auto &entry : qAsConst(changedEntries)) {
        switch (entry.status()) {
        case KNS3::Entry::Installed:
            for (const auto &file : entry.installedFiles()) {
                if (ColorSchemeManager::instance()->loadColorScheme(file)) {
                    continue;
                }
                qWarning() << "Failed to load file" << file;
                ++failures;
            }
            if (failures == entry.installedFiles().size()) {
                _appearanceUi->colorSchemeMessageWidget->setText(
                            xi18nc("@info",
                                   "Scheme <resource>%1</resource> failed to load.",
                                   entry.name()));
                _appearanceUi->colorSchemeMessageWidget->animatedShow();
                QTimer::singleShot(8000,
                                   _appearanceUi->colorSchemeMessageWidget,
                                   &KMessageWidget::animatedHide);
            }
            break;
        case KNS3::Entry::Deleted:
            for (const auto &file : entry.uninstalledFiles()) {
                if (ColorSchemeManager::instance()->unloadColorScheme(file)) {
                    continue;
                }
                qWarning() << "Failed to unload file" << file;
                // If unloading fails we do not care. If the scheme failed here
                // it either wasn't loaded or was invalid to begin with.
            }
            break;
        case KNS3::Entry::Invalid:
        case KNS3::Entry::Installing:
        case KNS3::Entry::Downloadable:
        case KNS3::Entry::Updateable:
        case KNS3::Entry::Updating:
            // Not interesting.
            break;
        }
    }
    updateColorSchemeList(currentColorSchemeName());
}

void EditProfileDialog::resetColorScheme()
{

    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const ColorScheme *>()->name();

        ColorSchemeManager::instance()->deleteColorScheme(name);

        // select the colorScheme used in the current profile
        updateColorSchemeList(currentColorSchemeName());
    }
}

void EditProfileDialog::showColorSchemeEditor(bool isNewScheme)
{
    // Finding selected ColorScheme
    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();
    QAbstractItemModel *model = _appearanceUi->colorSchemeList->model();
    const ColorScheme *colors = nullptr;
    if (!selected.isEmpty()) {
        colors = model->data(selected.first(), Qt::UserRole + 1).value<const ColorScheme *>();
    } else {
        colors = ColorSchemeManager::instance()->defaultColorScheme();
    }

    Q_ASSERT(colors);

    // Setting up ColorSchemeEditor ui
    // close any running ColorSchemeEditor
    if (_colorDialog != nullptr) {
        closeColorSchemeEditor();
    }
    _colorDialog = new ColorSchemeEditor(this);

    connect(_colorDialog, &Konsole::ColorSchemeEditor::colorSchemeSaveRequested, this,
            &Konsole::EditProfileDialog::saveColorScheme);
    _colorDialog->setup(colors, isNewScheme);

    _colorDialog->show();
}

void EditProfileDialog::closeColorSchemeEditor()
{
    if (_colorDialog != nullptr) {
        _colorDialog->close();
        delete _colorDialog;
    }
}

void EditProfileDialog::newColorScheme()
{
    showColorSchemeEditor(true);
}

void EditProfileDialog::editColorScheme()
{
    showColorSchemeEditor(false);
}

void EditProfileDialog::saveColorScheme(const ColorScheme &scheme, bool isNewScheme)
{
    auto newScheme = new ColorScheme(scheme);

    // if this is a new color scheme, pick a name based on the description
    if (isNewScheme) {
        newScheme->setName(newScheme->description());
    }

    ColorSchemeManager::instance()->addColorScheme(newScheme);

    const QString &selectedColorSchemeName = newScheme->name();

    // select the edited or the new colorScheme after saving the changes
    updateColorSchemeList(selectedColorSchemeName);

    preview(Profile::ColorScheme, newScheme->name());
}

void EditProfileDialog::colorSchemeSelected()
{
    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        QAbstractItemModel *model = _appearanceUi->colorSchemeList->model();
        const auto *colors = model->data(selected.first(), Qt::UserRole + 1).value<const ColorScheme *>();
        if (colors != nullptr) {
            updateTempProfileProperty(Profile::ColorScheme, colors->name());
            previewColorScheme(selected.first());

            updateTransparencyWarning();
        }
    }

    updateColorSchemeButtons();
}

void EditProfileDialog::updateColorSchemeButtons()
{
    enableIfNonEmptySelection(_appearanceUi->editColorSchemeButton, _appearanceUi->colorSchemeList->selectionModel());

    QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const ColorScheme *>()->name();

        bool isResettable = ColorSchemeManager::instance()->canResetColorScheme(name);
        _appearanceUi->resetColorSchemeButton->setEnabled(isResettable);

        bool isDeletable = ColorSchemeManager::instance()->isColorSchemeDeletable(name);
        // if a colorScheme can be restored then it can't be deleted
        _appearanceUi->removeColorSchemeButton->setEnabled(isDeletable && !isResettable);
    } else {
        _appearanceUi->removeColorSchemeButton->setEnabled(false);
        _appearanceUi->resetColorSchemeButton->setEnabled(false);
    }

}

void EditProfileDialog::updateKeyBindingsButtons()
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        _keyboardUi->editKeyBindingsButton->setEnabled(true);

        const QString &name = selected.first().data(Qt::UserRole + 1).value<const KeyboardTranslator *>()->name();

        bool isResettable = _keyManager->isTranslatorResettable(name);
        _keyboardUi->resetKeyBindingsButton->setEnabled(isResettable);

        bool isDeletable = _keyManager->isTranslatorDeletable(name);

        // if a key bindings scheme can be reset then it can't be deleted
        _keyboardUi->removeKeyBindingsButton->setEnabled(isDeletable && !isResettable);
    }
}

void EditProfileDialog::enableIfNonEmptySelection(QWidget *widget, QItemSelectionModel *selectionModel)
{
    widget->setEnabled(selectionModel->hasSelection());
}

void EditProfileDialog::updateTransparencyWarning()
{
    // zero or one indexes can be selected
    const QModelIndexList selected = _appearanceUi->colorSchemeList->selectionModel()->selectedIndexes();
    for (const QModelIndex &index : selected) {
        bool needTransparency = index.data(Qt::UserRole + 1).value<const ColorScheme *>()->opacity() < 1.0;

        if (!needTransparency) {
            _appearanceUi->transparencyWarningWidget->setHidden(true);
        } else if (!KWindowSystem::compositingActive()) {
            _appearanceUi->transparencyWarningWidget->setText(i18n(
                                                        "This color scheme uses a transparent background"
                                                        " which does not appear to be supported on your"
                                                        " desktop"));
            _appearanceUi->transparencyWarningWidget->setHidden(false);
        } else if (!WindowSystemInfo::HAVE_TRANSPARENCY) {
            _appearanceUi->transparencyWarningWidget->setText(i18n(
                                                        "Konsole was started before desktop effects were enabled."
                                                        " You need to restart Konsole to see transparent background."));
            _appearanceUi->transparencyWarningWidget->setHidden(false);
        }
    }
}

void EditProfileDialog::createTempProfile()
{
    _tempProfile = Profile::Ptr(new Profile);
    _tempProfile->setHidden(true);
}

void EditProfileDialog::updateTempProfileProperty(Profile::Property property, const QVariant &value)
{
    _tempProfile->setProperty(property, value);
    updateButtonApply();
}

void EditProfileDialog::updateButtonApply()
{
    bool userModified = false;

    QHashIterator<Profile::Property, QVariant> iter(_tempProfile->setProperties());
    while (iter.hasNext()) {
        iter.next();

        Profile::Property property = iter.key();
        QVariant value = iter.value();

        // for previewed property
        if (_previewedProperties.contains(static_cast<int>(property))) {
            if (value != _previewedProperties.value(static_cast<int>(property))) {
                userModified = true;
                break;
            }
        // for not-previewed property
        //
        // for the Profile::KeyBindings property, if it's set in the _tempProfile
        // then the user opened the edit key bindings dialog and clicked
        // OK, and could have add/removed a key bindings rule
        } else if (property == Profile::KeyBindings || (value != _profile->property<QVariant>(property))) {
            userModified = true;
            break;
        }
    }

    _buttonBox->button(QDialogButtonBox::Apply)->setEnabled(userModified);
}

void EditProfileDialog::setupKeyboardPage(const Profile::Ptr &/* profile */)
{
    // setup translator list
    updateKeyBindingsList(lookupProfile()->keyBindings());

    connect(_keyboardUi->keyBindingList->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            &Konsole::EditProfileDialog::keyBindingSelected);
    connect(_keyboardUi->newKeyBindingsButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::newKeyBinding);

    _keyboardUi->editKeyBindingsButton->setEnabled(false);
    _keyboardUi->removeKeyBindingsButton->setEnabled(false);
    _keyboardUi->resetKeyBindingsButton->setEnabled(false);

    updateKeyBindingsButtons();

    connect(_keyboardUi->editKeyBindingsButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::editKeyBinding);
    connect(_keyboardUi->removeKeyBindingsButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::removeKeyBinding);
    connect(_keyboardUi->resetKeyBindingsButton, &QPushButton::clicked, this,
            &Konsole::EditProfileDialog::resetKeyBindings);
}

void EditProfileDialog::keyBindingSelected()
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        QAbstractItemModel *model = _keyboardUi->keyBindingList->model();
        const auto *translator = model->data(selected.first(), Qt::UserRole + 1)
                                               .value<const KeyboardTranslator *>();
        if (translator != nullptr) {
            updateTempProfileProperty(Profile::KeyBindings, translator->name());
        }
    }

    updateKeyBindingsButtons();
}

void EditProfileDialog::removeKeyBinding()
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const KeyboardTranslator *>()->name();
        if (KeyboardTranslatorManager::instance()->deleteTranslator(name)) {
            _keyboardUi->keyBindingList->model()->removeRow(selected.first().row());
        }
    }
}

void EditProfileDialog::showKeyBindingEditor(bool isNewTranslator)
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();
    QAbstractItemModel *model = _keyboardUi->keyBindingList->model();

    const KeyboardTranslator *translator = nullptr;
    if (!selected.isEmpty()) {
        translator = model->data(selected.first(), Qt::UserRole + 1).value<const KeyboardTranslator *>();
    } else {
        translator = _keyManager->defaultTranslator();
    }

    Q_ASSERT(translator);

    auto editor = new KeyBindingEditor(this);

    if (translator != nullptr) {
        editor->setup(translator, lookupProfile()->keyBindings(), isNewTranslator);
    }

    connect(editor, &Konsole::KeyBindingEditor::updateKeyBindingsListRequest,
            this, &Konsole::EditProfileDialog::updateKeyBindingsList);

    connect(editor, &Konsole::KeyBindingEditor::updateTempProfileKeyBindingsRequest,
            this, &Konsole::EditProfileDialog::updateTempProfileProperty);

    editor->exec();
}

void EditProfileDialog::newKeyBinding()
{
    showKeyBindingEditor(true);
}

void EditProfileDialog::editKeyBinding()
{
    showKeyBindingEditor(false);
}

void EditProfileDialog::resetKeyBindings()
{
    QModelIndexList selected = _keyboardUi->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString &name = selected.first().data(Qt::UserRole + 1).value<const KeyboardTranslator *>()->name();

        _keyManager->deleteTranslator(name);
        // find and load the translator
        _keyManager->findTranslator(name);

        updateKeyBindingsList(name);
    }
}

void EditProfileDialog::setupButtonGroup(const ButtonGroupOptions &options, const Profile::Ptr &profile)
{
    auto currentValue = profile->property<int>(options.profileProperty);

    for (auto option: options.buttons) {
        options.group->setId(option.button, option.value);
    }

    Q_ASSERT(options.buttons.count() > 0);
    auto *activeButton = options.group->button(currentValue);
    if (activeButton == nullptr) {
        activeButton = options.buttons[0].button;
    }
    activeButton->setChecked(true);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 15, 0))
    connect(options.group, QOverload<int>::of(&QButtonGroup::idClicked),
#else
    connect(options.group, QOverload<int>::of(&QButtonGroup::buttonClicked),
#endif
            this, [this, options](int value) {
        if (options.preview) {
            preview(options.profileProperty, value);
        }
        updateTempProfileProperty(options.profileProperty, value);
    });
}

void EditProfileDialog::setupScrollingPage(const Profile::Ptr &profile)
{
    // setup scrollbar radio
    const ButtonGroupOptions scrollBarPositionOptions = {
        _scrollingUi->scrollBarPosition,    // group
        Profile::ScrollBarPosition,         // profileProperty
        false,                              // preview
        {                                   // buttons
            {_scrollingUi->scrollBarRightButton,    Enum::ScrollBarRight},
            {_scrollingUi->scrollBarLeftButton,     Enum::ScrollBarLeft},
            {_scrollingUi->scrollBarHiddenButton,   Enum::ScrollBarHidden},
        },
    };
    setupButtonGroup(scrollBarPositionOptions, profile);

    // setup scrollback type radio
    auto scrollBackType = profile->property<int>(Profile::HistoryMode);
    _scrollingUi->historySizeWidget->setMode(Enum::HistoryModeEnum(scrollBackType));
    connect(_scrollingUi->historySizeWidget, &Konsole::HistorySizeWidget::historyModeChanged, this,
            &Konsole::EditProfileDialog::historyModeChanged);

    // setup scrollback line count spinner
    const int historySize = profile->historySize();
    _scrollingUi->historySizeWidget->setLineCount(historySize);

    // setup scrollpageamount type radio
    auto scrollFullPage = profile->property<int>(Profile::ScrollFullPage);

    _scrollingUi->scrollHalfPage->setChecked(Enum::ScrollPageHalf == scrollFullPage);
    connect(_scrollingUi->scrollHalfPage, &QPushButton::clicked, this, &EditProfileDialog::scrollFullPage);

    _scrollingUi->scrollFullPage->setChecked(Enum::ScrollPageFull == scrollFullPage);
    connect(_scrollingUi->scrollFullPage, &QPushButton::clicked, this, &EditProfileDialog::scrollFullPage);

    _scrollingUi->highlightScrolledLinesButton->setChecked(profile->property<bool>(Profile::HighlightScrolledLines));
    connect(_scrollingUi->highlightScrolledLinesButton, &QPushButton::clicked, this,  &EditProfileDialog::toggleHighlightScrolledLines);

    // signals and slots
    connect(_scrollingUi->historySizeWidget, &Konsole::HistorySizeWidget::historySizeChanged, this,
            &Konsole::EditProfileDialog::historySizeChanged);
}

void EditProfileDialog::historySizeChanged(int lineCount)
{
    updateTempProfileProperty(Profile::HistorySize, lineCount);
}

void EditProfileDialog::historyModeChanged(Enum::HistoryModeEnum mode)
{
    updateTempProfileProperty(Profile::HistoryMode, mode);
}

void EditProfileDialog::scrollFullPage()
{
    updateTempProfileProperty(Profile::ScrollFullPage, Enum::ScrollPageFull);
}

void EditProfileDialog::scrollHalfPage()
{
    updateTempProfileProperty(Profile::ScrollFullPage, Enum::ScrollPageHalf);
}

void EditProfileDialog::toggleHighlightScrolledLines(bool enable)
{
    updateTempProfileProperty(Profile::HighlightScrolledLines, enable);
}

void EditProfileDialog::setupMousePage(const Profile::Ptr &profile)
{
    _mouseUi->underlineLinksButton->setChecked(profile->property<bool>(Profile::UnderlineLinksEnabled));
    connect(_mouseUi->underlineLinksButton, &QPushButton::toggled, this, &EditProfileDialog::toggleUnderlineLinks);
    _mouseUi->underlineFilesButton->setChecked(profile->property<bool>(Profile::UnderlineFilesEnabled));
    connect(_mouseUi->underlineFilesButton, &QPushButton::toggled, this, &EditProfileDialog::toggleUnderlineFiles);
    _mouseUi->ctrlRequiredForDragButton->setChecked(profile->property<bool>(Profile::CtrlRequiredForDrag));
    connect(_mouseUi->ctrlRequiredForDragButton, &QPushButton::toggled, this, &EditProfileDialog::toggleCtrlRequiredForDrag);
    _mouseUi->copyTextAsHTMLButton->setChecked(profile->property<bool>(Profile::CopyTextAsHTML));
    connect(_mouseUi->copyTextAsHTMLButton, &QPushButton::toggled, this, &EditProfileDialog::toggleCopyTextAsHTML);
    _mouseUi->copyTextToClipboardButton->setChecked(profile->property<bool>(Profile::AutoCopySelectedText));
    connect(_mouseUi->copyTextToClipboardButton, &QPushButton::toggled, this, &EditProfileDialog::toggleCopyTextToClipboard);
    _mouseUi->trimLeadingSpacesButton->setChecked(profile->property<bool>(Profile::TrimLeadingSpacesInSelectedText));
    connect(_mouseUi->trimLeadingSpacesButton, &QPushButton::toggled, this, &EditProfileDialog::toggleTrimLeadingSpacesInSelectedText);
    _mouseUi->trimTrailingSpacesButton->setChecked(profile->property<bool>(Profile::TrimTrailingSpacesInSelectedText));
    connect(_mouseUi->trimTrailingSpacesButton, &QPushButton::toggled, this, &EditProfileDialog::toggleTrimTrailingSpacesInSelectedText);
    _mouseUi->openLinksByDirectClickButton->setChecked(profile->property<bool>(Profile::OpenLinksByDirectClickEnabled));
    connect(_mouseUi->openLinksByDirectClickButton, &QPushButton::toggled, this, &EditProfileDialog::toggleOpenLinksByDirectClick);
    _mouseUi->dropUrlsAsText->setChecked(profile->property<bool>(Profile::DropUrlsAsText));
    connect(_mouseUi->dropUrlsAsText, &QPushButton::toggled, this, &EditProfileDialog::toggleDropUrlsAsText);
    _mouseUi->enableAlternateScrollingButton->setChecked(profile->property<bool>(Profile::AlternateScrolling));
    connect(_mouseUi->enableAlternateScrollingButton, &QPushButton::toggled, this, &EditProfileDialog::toggleAlternateScrolling);

    // setup middle click paste mode
    const auto middleClickPasteMode = profile->property<int>(Profile::MiddleClickPasteMode);
    _mouseUi->pasteFromX11SelectionButton->setChecked(Enum::PasteFromX11Selection == middleClickPasteMode);
    connect(_mouseUi->pasteFromX11SelectionButton, &QPushButton::clicked, this, &EditProfileDialog::pasteFromX11Selection);
    _mouseUi->pasteFromClipboardButton->setChecked(Enum::PasteFromClipboard == middleClickPasteMode);
    connect(_mouseUi->pasteFromClipboardButton, &QPushButton::clicked, this, &EditProfileDialog::pasteFromClipboard);

    // interaction options
    _mouseUi->wordCharacterEdit->setText(profile->wordCharacters());

    connect(_mouseUi->wordCharacterEdit, &QLineEdit::textChanged, this, &Konsole::EditProfileDialog::wordCharactersChanged);

    const ButtonGroupOptions tripleClickModeOptions = {
        _mouseUi->tripleClickMode,  // group
        Profile::TripleClickMode,   // profileProperty
        false,                      // preview
        {                           // buttons
            {_mouseUi->tripleClickSelectsTheWholeLine,      Enum::SelectWholeLine},
            {_mouseUi->tripleClickSelectsFromMousePosition, Enum::SelectForwardsFromCursor},
        },
    };
    setupButtonGroup(tripleClickModeOptions, profile);

    _mouseUi->openLinksByDirectClickButton->setEnabled(_mouseUi->underlineLinksButton->isChecked() || _mouseUi->underlineFilesButton->isChecked());

    _mouseUi->enableMouseWheelZoomButton->setChecked(profile->mouseWheelZoomEnabled());
    connect(_mouseUi->enableMouseWheelZoomButton, &QCheckBox::toggled, this, &Konsole::EditProfileDialog::toggleMouseWheelZoom);

    _mouseUi->allowLinkEscapeSequenceButton->setChecked(profile->allowEscapedLinks());
    connect(_mouseUi->allowLinkEscapeSequenceButton, &QPushButton::clicked,
            this, &Konsole::EditProfileDialog::toggleAllowLinkEscapeSequence);

    _mouseUi->linkEscapeSequenceTexts->setEnabled(profile->allowEscapedLinks());
    _mouseUi->linkEscapeSequenceTexts->setText(profile->escapedLinksSchema().join(QLatin1Char(';')));
    connect(_mouseUi->linkEscapeSequenceTexts, &QLineEdit::textChanged,
            this, &Konsole::EditProfileDialog::linkEscapeSequenceTextsChanged);
}

void EditProfileDialog::setupAdvancedPage(const Profile::Ptr &profile)
{
    _advancedUi->enableBlinkingTextButton->setChecked(profile->property<bool>(Profile::BlinkingTextEnabled));
    connect(_advancedUi->enableBlinkingTextButton, &QPushButton::toggled, this, &EditProfileDialog::toggleBlinkingText);
    _advancedUi->enableFlowControlButton->setChecked(profile->property<bool>(Profile::FlowControlEnabled));
    connect(_advancedUi->enableFlowControlButton, &QPushButton::toggled, this, &EditProfileDialog::toggleFlowControl);
    _appearanceUi->enableBlinkingCursorButton->setChecked(profile->property<bool>(Profile::BlinkingCursorEnabled));
    connect(_appearanceUi->enableBlinkingCursorButton, &QPushButton::toggled, this, &EditProfileDialog::toggleBlinkingCursor);
    _advancedUi->enableBidiRenderingButton->setChecked(profile->property<bool>(Profile::BidiRenderingEnabled));
    connect(_advancedUi->enableBidiRenderingButton, &QPushButton::toggled, this, &EditProfileDialog::togglebidiRendering);
    _advancedUi->enableReverseUrlHints->setChecked(profile->property<bool>(Profile::ReverseUrlHints));
    connect(_advancedUi->enableReverseUrlHints, &QPushButton::toggled, this, &EditProfileDialog::toggleReverseUrlHints);

    // Setup the URL hints modifier checkboxes
    {
        auto modifiers = profile->property<int>(Profile::UrlHintsModifiers);
        _advancedUi->urlHintsModifierShift->setChecked((modifiers &Qt::ShiftModifier) != 0U);
        _advancedUi->urlHintsModifierCtrl->setChecked((modifiers &Qt::ControlModifier) != 0U);
        _advancedUi->urlHintsModifierAlt->setChecked((modifiers &Qt::AltModifier) != 0U);
        _advancedUi->urlHintsModifierMeta->setChecked((modifiers &Qt::MetaModifier) != 0U);
        connect(_advancedUi->urlHintsModifierShift, &QCheckBox::toggled, this, &EditProfileDialog::updateUrlHintsModifier);
        connect(_advancedUi->urlHintsModifierCtrl, &QCheckBox::toggled, this, &EditProfileDialog::updateUrlHintsModifier);
        connect(_advancedUi->urlHintsModifierAlt, &QCheckBox::toggled, this, &EditProfileDialog::updateUrlHintsModifier);
        connect(_advancedUi->urlHintsModifierMeta, &QCheckBox::toggled, this, &EditProfileDialog::updateUrlHintsModifier);
    }

    // encoding options
    auto codecAction = new KCodecAction(this);
    codecAction->setCurrentCodec(profile->defaultEncoding());
    _advancedUi->selectEncodingButton->setMenu(codecAction->menu());
    connect(codecAction,
            QOverload<QTextCodec *>::of(&KCodecAction::triggered), this,
            &Konsole::EditProfileDialog::setDefaultCodec);

    _advancedUi->selectEncodingButton->setText(profile->defaultEncoding());

    _advancedUi->peekPrimaryWidget->setKeySequence(profile->peekPrimaryKeySequence());
    connect(_advancedUi->peekPrimaryWidget, &QKeySequenceEdit::editingFinished, this, &EditProfileDialog::peekPrimaryKeySequenceChanged);
}

int EditProfileDialog::maxSpinBoxWidth(const KPluralHandlingSpinBox *spinBox, const KLocalizedString &suffix)
{
    static const int cursorWidth = 2;

    const QFontMetrics fm(spinBox->fontMetrics());
    const QString plural    = suffix.subs(2).toString();
    const QString singular  = suffix.subs(1).toString();
    const QString min       = QString::number(spinBox->minimum());
    const QString max       = QString::number(spinBox->maximum());
    const int pluralWidth   = fm.boundingRect(plural).width();
    const int singularWidth = fm.boundingRect(singular).width();
    const int minWidth      = fm.boundingRect(min).width();
    const int maxWidth      = fm.boundingRect(max).width();
    const int width         = qMax(pluralWidth, singularWidth) + qMax(minWidth, maxWidth) + cursorWidth;

    // Based on QAbstractSpinBox::initStyleOption() from Qt
    QStyleOptionSpinBox opt;
    opt.initFrom(spinBox);
    opt.activeSubControls   = QStyle::SC_None;
    opt.buttonSymbols       = spinBox->buttonSymbols();
    // Assume all spinboxes have buttons
    opt.subControls         = QStyle::SC_SpinBoxFrame | QStyle::SC_SpinBoxEditField
                            | QStyle::SC_SpinBoxUp | QStyle::SC_SpinBoxDown;
    opt.frame               = spinBox->hasFrame();

    const QSize hint(width, spinBox->sizeHint().height());
    const QSize spinBoxSize = style()->sizeFromContents(QStyle::CT_SpinBox, &opt, hint, spinBox)
                              .expandedTo(QApplication::globalStrut());

    return spinBoxSize.width();
}

void EditProfileDialog::setDefaultCodec(QTextCodec *codec)
{
    QString name = QString::fromLocal8Bit(codec->name());

    updateTempProfileProperty(Profile::DefaultEncoding, name);
    _advancedUi->selectEncodingButton->setText(name);
}

void EditProfileDialog::wordCharactersChanged(const QString &text)
{
    updateTempProfileProperty(Profile::WordCharacters, text);
}

void EditProfileDialog::togglebidiRendering(bool enable)
{
    updateTempProfileProperty(Profile::BidiRenderingEnabled, enable);
}

void EditProfileDialog::toggleUnderlineLinks(bool enable)
{
    updateTempProfileProperty(Profile::UnderlineLinksEnabled, enable);

    bool enableClick = _mouseUi->underlineFilesButton->isChecked() || enable;
    _mouseUi->openLinksByDirectClickButton->setEnabled(enableClick);
}

void EditProfileDialog::toggleUnderlineFiles(bool enable)
{
    updateTempProfileProperty(Profile::UnderlineFilesEnabled, enable);

    bool enableClick = _mouseUi->underlineLinksButton->isChecked() || enable;
    _mouseUi->openLinksByDirectClickButton->setEnabled(enableClick);
}

void EditProfileDialog::toggleCtrlRequiredForDrag(bool enable)
{
    updateTempProfileProperty(Profile::CtrlRequiredForDrag, enable);
}

void EditProfileDialog::toggleDropUrlsAsText(bool enable)
{
    updateTempProfileProperty(Profile::DropUrlsAsText, enable);
}

void EditProfileDialog::toggleOpenLinksByDirectClick(bool enable)
{
    updateTempProfileProperty(Profile::OpenLinksByDirectClickEnabled, enable);
}

void EditProfileDialog::toggleCopyTextAsHTML(bool enable)
{
    updateTempProfileProperty(Profile::CopyTextAsHTML, enable);
}

void EditProfileDialog::toggleCopyTextToClipboard(bool enable)
{
    updateTempProfileProperty(Profile::AutoCopySelectedText, enable);
}

void EditProfileDialog::toggleTrimLeadingSpacesInSelectedText(bool enable)
{
    updateTempProfileProperty(Profile::TrimLeadingSpacesInSelectedText, enable);
}

void EditProfileDialog::toggleTrimTrailingSpacesInSelectedText(bool enable)
{
    updateTempProfileProperty(Profile::TrimTrailingSpacesInSelectedText, enable);
}

void EditProfileDialog::pasteFromX11Selection()
{
    updateTempProfileProperty(Profile::MiddleClickPasteMode, Enum::PasteFromX11Selection);
}

void EditProfileDialog::pasteFromClipboard()
{
    updateTempProfileProperty(Profile::MiddleClickPasteMode, Enum::PasteFromClipboard);
}

void EditProfileDialog::TripleClickModeChanged(int newValue)
{
    updateTempProfileProperty(Profile::TripleClickMode, newValue);
}

void EditProfileDialog::updateUrlHintsModifier(bool)
{
    Qt::KeyboardModifiers modifiers;
    if (_advancedUi->urlHintsModifierShift->isChecked()) {
        modifiers |= Qt::ShiftModifier;
    }
    if (_advancedUi->urlHintsModifierCtrl->isChecked()) {
        modifiers |= Qt::ControlModifier;
    }
    if (_advancedUi->urlHintsModifierAlt->isChecked()) {
        modifiers |= Qt::AltModifier;
    }
    if (_advancedUi->urlHintsModifierMeta->isChecked()) {
        modifiers |= Qt::MetaModifier;
    }
    updateTempProfileProperty(Profile::UrlHintsModifiers, int(modifiers));
}

void EditProfileDialog::toggleReverseUrlHints(bool enable)
{
    updateTempProfileProperty(Profile::ReverseUrlHints, enable);
}

void EditProfileDialog::toggleBlinkingText(bool enable)
{
    updateTempProfileProperty(Profile::BlinkingTextEnabled, enable);
}

void EditProfileDialog::toggleFlowControl(bool enable)
{
    updateTempProfileProperty(Profile::FlowControlEnabled, enable);
}

void EditProfileDialog::peekPrimaryKeySequenceChanged()
{
    updateTempProfileProperty(Profile::PeekPrimaryKeySequence, _advancedUi->peekPrimaryWidget->keySequence().toString());
}
