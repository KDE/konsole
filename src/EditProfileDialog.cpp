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

// Own
#include "EditProfileDialog.h"

// Standard
#include <cmath>

// Qt
#include <QtGui/QBrush>
#include <QtGui/QPainter>
#include <QStandardItem>
#include <QtCore/QTextCodec>
#include <QtGui/QLinearGradient>
#include <QtGui/QRadialGradient>
#include <QtCore/QTimer>
#include <QtCore/QTimeLine>

// KDE
#include <kdeversion.h>
#if KDE_IS_VERSION(4, 9, 1)
#include <KCodecAction>
#else
#include <kcodecaction.h>
#endif

#include <KFontDialog>
#include <KIcon>
#include <KIconDialog>
#include <KFileDialog>
#include <KUrlCompletion>
#include <KWindowSystem>
#include <KTextEdit>
#include <KMessageBox>

// Konsole
#include "ColorScheme.h"
#include "ColorSchemeManager.h"
#include "ColorSchemeEditor.h"
#include "ui_EditProfileDialog.h"
#include "KeyBindingEditor.h"
#include "KeyboardTranslator.h"
#include "KeyboardTranslatorManager.h"
#include "ProfileManager.h"
#include "ShellCommand.h"
#include "WindowSystemInfo.h"
#include "Enumeration.h"

using namespace Konsole;

EditProfileDialog::EditProfileDialog(QWidget* aParent)
    : KDialog(aParent)
    , _colorSchemeAnimationTimeLine(0)
    , _delayedPreviewTimer(new QTimer(this))
{
    setCaption(i18n("Edit Profile"));
    setButtons(KDialog::Ok | KDialog::Cancel | KDialog::Apply);

    // disable the apply button , since no modification has been made
    enableButtonApply(false);

    connect(this, SIGNAL(applyClicked()), this, SLOT(save()));

    connect(_delayedPreviewTimer, SIGNAL(timeout()), this, SLOT(delayedPreviewActivate()));

    _ui = new Ui::EditProfileDialog();
    _ui->setupUi(mainWidget());

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
    _pageNeedsUpdate.resize(_ui->tabWidget->count());
    connect(_ui->tabWidget, SIGNAL(currentChanged(int)), this,
            SLOT(preparePage(int)));

    createTempProfile();
}
EditProfileDialog::~EditProfileDialog()
{
    delete _ui;
}
void EditProfileDialog::save()
{
    if (_tempProfile->isEmpty())
        return;

    ProfileManager::instance()->changeProfile(_profile, _tempProfile->setProperties());

    // ensure that these settings are not undone by a call
    // to unpreview()
    QHashIterator<Profile::Property, QVariant> iter(_tempProfile->setProperties());
    while (iter.hasNext()) {
        iter.next();
        _previewedProperties.remove(iter.key());
    }

    createTempProfile();

    enableButtonApply(false);
}
void EditProfileDialog::reject()
{
    unpreviewAll();
    KDialog::reject();
}
void EditProfileDialog::accept()
{
    Q_ASSERT(_profile);
    Q_ASSERT(_tempProfile);

    if ((_tempProfile->isPropertySet(Profile::Name) &&
            _tempProfile->name().isEmpty())
            || (_profile->name().isEmpty() && _tempProfile->name().isEmpty())) {
        KMessageBox::sorry(this,
                           i18n("<p>Each profile must have a name before it can be saved "
                                "into disk.</p>"));
        return;
    }
    save();
    unpreviewAll();
    KDialog::accept();
}
QString EditProfileDialog::groupProfileNames(const ProfileGroup::Ptr group, int maxLength)
{
    QString caption;
    int count = group->profiles().count();
    for (int i = 0; i < count; i++) {
        caption += group->profiles()[i]->name();
        if (i < (count - 1)) {
            caption += ',';
            // limit caption length to prevent very long window titles
            if (maxLength > 0 && caption.length() > maxLength) {
                caption += "...";
                break;
            }
        }
    }
    return caption;
}
void EditProfileDialog::updateCaption(const Profile::Ptr profile)
{
    const int MAX_GROUP_CAPTION_LENGTH = 25;
    ProfileGroup::Ptr group = profile->asGroup();
    if (group && group->profiles().count() > 1) {
        QString caption = groupProfileNames(group, MAX_GROUP_CAPTION_LENGTH);
        setCaption(i18np("Editing profile: %2",
                         "Editing %1 profiles: %2",
                         group->profiles().count(),
                         caption));
    } else {
        setCaption(i18n("Edit Profile \"%1\"", profile->name()));
    }
}
void EditProfileDialog::setProfile(Profile::Ptr profile)
{
    Q_ASSERT(profile);

    _profile = profile;

    // update caption
    updateCaption(profile);

    // mark each page of the dialog as out of date
    // and force an update of the currently visible page
    //
    // the other pages will be updated as necessary
    _pageNeedsUpdate.fill(true);
    preparePage(_ui->tabWidget->currentIndex());

    if (_tempProfile) {
        createTempProfile();
    }
}
const Profile::Ptr EditProfileDialog::lookupProfile() const
{
    return _profile;
}
void EditProfileDialog::preparePage(int page)
{
    const Profile::Ptr profile = lookupProfile();

    Q_ASSERT(_pageNeedsUpdate.count() > page);
    Q_ASSERT(profile);

    QWidget* pageWidget = _ui->tabWidget->widget(page);

    if (_pageNeedsUpdate[page]) {
        if (pageWidget == _ui->generalTab)
            setupGeneralPage(profile);
        else if (pageWidget == _ui->tabsTab)
            setupTabsPage(profile);
        else if (pageWidget == _ui->appearanceTab)
            setupAppearancePage(profile);
        else if (pageWidget == _ui->scrollingTab)
            setupScrollingPage(profile);
        else if (pageWidget == _ui->keyboardTab)
            setupKeyboardPage(profile);
        else if (pageWidget == _ui->mouseTab)
            setupMousePage(profile);
        else if (pageWidget == _ui->advancedTab)
            setupAdvancedPage(profile);
        else
            Q_ASSERT(false);

        _pageNeedsUpdate[page] = false;
    }

    // start page entry animation for color schemes
    if (pageWidget == _ui->appearanceTab)
        _colorSchemeAnimationTimeLine->start();
}
void EditProfileDialog::selectProfileName()
{
    _ui->profileNameEdit->setFocus();
    _ui->profileNameEdit->selectAll();
}
void EditProfileDialog::setupGeneralPage(const Profile::Ptr profile)
{
    // basic profile options
    {
        _ui->emptyNameWarningWidget->setWordWrap(false);
        _ui->emptyNameWarningWidget->setCloseButtonVisible(false);
        _ui->emptyNameWarningWidget->setMessageType(KMessageWidget::Warning);

        ProfileGroup::Ptr group = profile->asGroup();
        if (!group || group->profiles().count() < 2) {
            _ui->profileNameEdit->setText(profile->name());
            _ui->profileNameEdit->setClearButtonShown(true);

            _ui->emptyNameWarningWidget->setVisible(profile->name().isEmpty());
            _ui->emptyNameWarningWidget->setText(i18n("Profile name is empty."));
        } else {
            _ui->profileNameEdit->setText(groupProfileNames(group, -1));
            _ui->profileNameEdit->setEnabled(false);
            _ui->profileNameLabel->setEnabled(false);

            _ui->emptyNameWarningWidget->setVisible(false);
        }
    }

    ShellCommand command(profile->command() , profile->arguments());
    _ui->commandEdit->setText(command.fullCommand());
    KUrlCompletion* exeCompletion = new KUrlCompletion(KUrlCompletion::ExeCompletion);
    exeCompletion->setParent(this);
    exeCompletion->setDir(QString());
    _ui->commandEdit->setCompletionObject(exeCompletion);

    _ui->initialDirEdit->setText(profile->defaultWorkingDirectory());
    KUrlCompletion* dirCompletion = new KUrlCompletion(KUrlCompletion::DirCompletion);
    dirCompletion->setParent(this);
    _ui->initialDirEdit->setCompletionObject(dirCompletion);
    _ui->initialDirEdit->setClearButtonShown(true);

    _ui->dirSelectButton->setIcon(KIcon("folder-open"));
    _ui->iconSelectButton->setIcon(KIcon(profile->icon()));
    _ui->startInSameDirButton->setChecked(profile->startInCurrentSessionDir());

    // window options
    _ui->showTerminalSizeHintButton->setChecked(profile->showTerminalSizeHint());
    _ui->saveGeometryOnExitButton->setChecked(profile->saveGeometryOnExit());

    // signals and slots
    connect(_ui->dirSelectButton, SIGNAL(clicked()), this, SLOT(selectInitialDir()));
    connect(_ui->iconSelectButton, SIGNAL(clicked()), this, SLOT(selectIcon()));
    connect(_ui->startInSameDirButton, SIGNAL(toggled(bool)), this ,
            SLOT(startInSameDir(bool)));
    connect(_ui->profileNameEdit, SIGNAL(textChanged(QString)), this,
            SLOT(profileNameChanged(QString)));
    connect(_ui->initialDirEdit, SIGNAL(textChanged(QString)), this,
            SLOT(initialDirChanged(QString)));
    connect(_ui->commandEdit, SIGNAL(textChanged(QString)), this,
            SLOT(commandChanged(QString)));
    connect(_ui->environmentEditButton , SIGNAL(clicked()), this,
            SLOT(showEnvironmentEditor()));

    connect(_ui->saveGeometryOnExitButton, SIGNAL(toggled(bool)), this,
            SLOT(saveGeometryOnExit(bool)));
    connect(_ui->showTerminalSizeHintButton, SIGNAL(toggled(bool)), this,
            SLOT(showTerminalSizeHint(bool)));
}
void EditProfileDialog::showEnvironmentEditor()
{
    const Profile::Ptr profile = lookupProfile();

    QWeakPointer<KDialog> dialog = new KDialog(this);
    KTextEdit* edit = new KTextEdit(dialog.data());

    QStringList currentEnvironment = profile->environment();

    edit->setPlainText(currentEnvironment.join("\n"));
    edit->setToolTip(i18n("One environment variable per line"));

    dialog.data()->setPlainCaption(i18n("Edit Environment"));
    dialog.data()->setMainWidget(edit);

    if (dialog.data()->exec() == QDialog::Accepted) {
        QStringList newEnvironment = edit->toPlainText().split('\n');
        updateTempProfileProperty(Profile::Environment, newEnvironment);
    }

    delete dialog.data();
}
void EditProfileDialog::setupTabsPage(const Profile::Ptr profile)
{
    // tab title format
    _ui->renameTabWidget->setTabTitleText(profile->localTabTitleFormat());
    _ui->renameTabWidget->setRemoteTabTitleText(profile->remoteTabTitleFormat());

    connect(_ui->renameTabWidget, SIGNAL(tabTitleFormatChanged(QString)), this,
            SLOT(tabTitleFormatChanged(QString)));
    connect(_ui->renameTabWidget, SIGNAL(remoteTabTitleFormatChanged(QString)), this,
            SLOT(remoteTabTitleFormatChanged(QString)));

    // tab monitoring
    const int silenceSeconds = profile->silenceSeconds();
    _ui->silenceSecondsSpinner->setValue(silenceSeconds);
    _ui->silenceSecondsSpinner->setSuffix(ki18ncp("Unit of time", " second", " seconds"));

    connect(_ui->silenceSecondsSpinner, SIGNAL(valueChanged(int)),
            this, SLOT(silenceSecondsChanged(int)));
}

void EditProfileDialog::saveGeometryOnExit(bool value)
{
    updateTempProfileProperty(Profile::SaveGeometryOnExit, value);
}
void EditProfileDialog::showTerminalSizeHint(bool value)
{
    updateTempProfileProperty(Profile::ShowTerminalSizeHint, value);
}
void EditProfileDialog::tabTitleFormatChanged(const QString& format)
{
    updateTempProfileProperty(Profile::LocalTabTitleFormat, format);
}
void EditProfileDialog::remoteTabTitleFormatChanged(const QString& format)
{
    updateTempProfileProperty(Profile::RemoteTabTitleFormat, format);
}

void EditProfileDialog::silenceSecondsChanged(int seconds)
{
    updateTempProfileProperty(Profile::SilenceSeconds, seconds);
}

void EditProfileDialog::selectIcon()
{
    const QString& icon = KIconDialog::getIcon(KIconLoader::Desktop, KIconLoader::Application,
                          false, 0, false, this);
    if (!icon.isEmpty()) {
        _ui->iconSelectButton->setIcon(KIcon(icon));
        updateTempProfileProperty(Profile::Icon, icon);
    }
}
void EditProfileDialog::profileNameChanged(const QString& text)
{
    _ui->emptyNameWarningWidget->setVisible(text.isEmpty());

    updateTempProfileProperty(Profile::Name, text);
    updateTempProfileProperty(Profile::UntranslatedName, text);
    updateCaption(_tempProfile);
}
void EditProfileDialog::startInSameDir(bool sameDir)
{
    updateTempProfileProperty(Profile::StartInCurrentSessionDir, sameDir);
}
void EditProfileDialog::initialDirChanged(const QString& dir)
{
    updateTempProfileProperty(Profile::Directory, dir);
}
void EditProfileDialog::commandChanged(const QString& command)
{
    ShellCommand shellCommand(command);

    updateTempProfileProperty(Profile::Command, shellCommand.command());
    updateTempProfileProperty(Profile::Arguments, shellCommand.arguments());
}
void EditProfileDialog::selectInitialDir()
{
    const KUrl url = KFileDialog::getExistingDirectoryUrl(_ui->initialDirEdit->text(),
                     this,
                     i18n("Select Initial Directory"));

    if (!url.isEmpty())
        _ui->initialDirEdit->setText(url.path());
}
void EditProfileDialog::setupAppearancePage(const Profile::Ptr profile)
{
    ColorSchemeViewDelegate* delegate = new ColorSchemeViewDelegate(this);
    _ui->colorSchemeList->setItemDelegate(delegate);

    _colorSchemeAnimationTimeLine = new QTimeLine(500 , this);
    delegate->setEntryTimeLine(_colorSchemeAnimationTimeLine);

    connect(_colorSchemeAnimationTimeLine, SIGNAL(valueChanged(qreal)), this,
            SLOT(colorSchemeAnimationUpdate()));

    _ui->transparencyWarningWidget->setVisible(false);
    _ui->transparencyWarningWidget->setWordWrap(true);
    _ui->transparencyWarningWidget->setCloseButtonVisible(false);
    _ui->transparencyWarningWidget->setMessageType(KMessageWidget::Warning);

    _ui->editColorSchemeButton->setEnabled(false);
    _ui->removeColorSchemeButton->setEnabled(false);

    // setup color list
    updateColorSchemeList(true);

    _ui->colorSchemeList->setMouseTracking(true);
    _ui->colorSchemeList->installEventFilter(this);
    _ui->colorSchemeList->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

    connect(_ui->colorSchemeList->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(colorSchemeSelected()));
    connect(_ui->colorSchemeList, SIGNAL(entered(QModelIndex)), this,
            SLOT(previewColorScheme(QModelIndex)));

    updateColorSchemeButtons();

    connect(_ui->editColorSchemeButton, SIGNAL(clicked()), this,
            SLOT(editColorScheme()));
    connect(_ui->removeColorSchemeButton, SIGNAL(clicked()), this,
            SLOT(removeColorScheme()));
    connect(_ui->newColorSchemeButton, SIGNAL(clicked()), this,
            SLOT(newColorScheme()));

    // setup font preview
    const bool antialias = profile->antiAliasFonts();

    QFont profileFont = profile->font();
    profileFont.setStyleStrategy(antialias ? QFont::PreferAntialias : QFont::NoAntialias);

    _ui->fontPreviewLabel->installEventFilter(this);
    _ui->fontPreviewLabel->setFont(profileFont);
    setFontInputValue(profileFont);

    connect(_ui->fontSizeInput, SIGNAL(valueChanged(double)), this,
            SLOT(setFontSize(double)));
    connect(_ui->selectFontButton, SIGNAL(clicked()), this,
            SLOT(showFontDialog()));

    // setup font smoothing
    _ui->antialiasTextButton->setChecked(antialias);
    connect(_ui->antialiasTextButton, SIGNAL(toggled(bool)), this,
            SLOT(setAntialiasText(bool)));

    _ui->boldIntenseButton->setChecked(profile->boldIntense());
    connect(_ui->boldIntenseButton, SIGNAL(toggled(bool)), this,
            SLOT(setBoldIntense(bool)));
}
void EditProfileDialog::setAntialiasText(bool enable)
{
    QFont profileFont = _ui->fontPreviewLabel->font();
    profileFont.setStyleStrategy(enable ? QFont::PreferAntialias : QFont::NoAntialias);

    // update preview to reflect text smoothing state
    fontSelected(profileFont);
    updateTempProfileProperty(Profile::AntiAliasFonts, enable);
}
void EditProfileDialog::setBoldIntense(bool enable)
{
    preview(Profile::BoldIntense, enable);
    updateTempProfileProperty(Profile::BoldIntense, enable);
}
void EditProfileDialog::colorSchemeAnimationUpdate()
{
    QAbstractItemModel* model = _ui->colorSchemeList->model();

    for (int i = model->rowCount() ; i >= 0 ; i--)
        _ui->colorSchemeList->update(model->index(i, 0));
}
void EditProfileDialog::updateColorSchemeList(bool selectCurrentScheme)
{
    if (!_ui->colorSchemeList->model())
        _ui->colorSchemeList->setModel(new QStandardItemModel(this));

    const QString& name = lookupProfile()->colorScheme();
    const ColorScheme* currentScheme = ColorSchemeManager::instance()->findColorScheme(name);

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(_ui->colorSchemeList->model());

    Q_ASSERT(model);

    model->clear();

    QStandardItem* selectedItem = 0;

    QList<const ColorScheme*> schemeList = ColorSchemeManager::instance()->allColorSchemes();

    foreach(const ColorScheme* scheme, schemeList) {
        QStandardItem* item = new QStandardItem(scheme->description());
        item->setData(QVariant::fromValue(scheme) ,  Qt::UserRole + 1);
        item->setFlags(item->flags());

        if (currentScheme == scheme)
            selectedItem = item;

        model->appendRow(item);
    }

    model->sort(0);

    if (selectCurrentScheme && selectedItem) {
        _ui->colorSchemeList->updateGeometry();
        _ui->colorSchemeList->selectionModel()->setCurrentIndex(selectedItem->index() ,
                QItemSelectionModel::Select);

        // update transparency warning label
        updateTransparencyWarning();
    }
}
void EditProfileDialog::updateKeyBindingsList(bool selectCurrentTranslator)
{
    if (!_ui->keyBindingList->model())
        _ui->keyBindingList->setModel(new QStandardItemModel(this));

    const QString& name = lookupProfile()->keyBindings();

    KeyboardTranslatorManager* keyManager = KeyboardTranslatorManager::instance();
    const KeyboardTranslator* currentTranslator = keyManager->findTranslator(name);

    QStandardItemModel* model = qobject_cast<QStandardItemModel*>(_ui->keyBindingList->model());

    Q_ASSERT(model);

    model->clear();

    QStandardItem* selectedItem = 0;

    QStringList translatorNames = keyManager->allTranslators();
    foreach(const QString& translatorName, translatorNames) {
        const KeyboardTranslator* translator = keyManager->findTranslator(translatorName);

        QStandardItem* item = new QStandardItem(translator->description());
        item->setEditable(false);
        item->setData(QVariant::fromValue(translator), Qt::UserRole + 1);
        item->setIcon(KIcon("preferences-desktop-keyboard"));

        if (translator == currentTranslator)
            selectedItem = item;

        model->appendRow(item);
    }

    model->sort(0);

    if (selectCurrentTranslator && selectedItem) {
        _ui->keyBindingList->selectionModel()->setCurrentIndex(selectedItem->index() ,
                QItemSelectionModel::Select);
    }
}
bool EditProfileDialog::eventFilter(QObject* watched , QEvent* aEvent)
{
    if (watched == _ui->colorSchemeList && aEvent->type() == QEvent::Leave) {
        if (_tempProfile->isPropertySet(Profile::ColorScheme))
            preview(Profile::ColorScheme, _tempProfile->colorScheme());
        else
            unpreview(Profile::ColorScheme);
    }
    if (watched == _ui->fontPreviewLabel && aEvent->type() == QEvent::FontChange) {
        const QFont& labelFont = _ui->fontPreviewLabel->font();
        _ui->fontPreviewLabel->setText(i18n("%1", labelFont.family()));
    }

    return KDialog::eventFilter(watched, aEvent);
}
void EditProfileDialog::unpreviewAll()
{
    _delayedPreviewTimer->stop();
    _delayedPreviewProperties.clear();

    QHash<Profile::Property, QVariant> map;
    QHashIterator<int, QVariant> iter(_previewedProperties);
    while (iter.hasNext()) {
        iter.next();
        map.insert((Profile::Property)iter.key(), iter.value());
    }

    // undo any preview changes
    if (!map.isEmpty())
        ProfileManager::instance()->changeProfile(_profile, map, false);
}
void EditProfileDialog::unpreview(int aProperty)
{
    _delayedPreviewProperties.remove(aProperty);

    if (!_previewedProperties.contains(aProperty))
        return;

    QHash<Profile::Property, QVariant> map;
    map.insert((Profile::Property)aProperty, _previewedProperties[aProperty]);
    ProfileManager::instance()->changeProfile(_profile, map, false);

    _previewedProperties.remove(aProperty);
}
void EditProfileDialog::delayedPreview(int aProperty , const QVariant& value)
{
    _delayedPreviewProperties.insert(aProperty, value);

    _delayedPreviewTimer->stop();
    _delayedPreviewTimer->start(300);
}
void EditProfileDialog::delayedPreviewActivate()
{
    Q_ASSERT(qobject_cast<QTimer*>(sender()));

    QMutableHashIterator<int, QVariant> iter(_delayedPreviewProperties);
    if (iter.hasNext()) {
        iter.next();
        preview(iter.key(), iter.value());
    }
}
void EditProfileDialog::preview(int aProperty , const QVariant& value)
{
    QHash<Profile::Property, QVariant> map;
    map.insert((Profile::Property)aProperty, value);

    _delayedPreviewProperties.remove(aProperty);

    const Profile::Ptr original = lookupProfile();

    // skip previews for profile groups if the profiles in the group
    // have conflicting original values for the property
    //
    // TODO - Save the original values for each profile and use to unpreview properties
    ProfileGroup::Ptr group = original->asGroup();
    if (group && group->profiles().count() > 1 &&
            original->property<QVariant>((Profile::Property)aProperty).isNull())
        return;

    if (!_previewedProperties.contains(aProperty)) {
        _previewedProperties.insert(aProperty , original->property<QVariant>((Profile::Property)aProperty));
    }

    // temporary change to color scheme
    ProfileManager::instance()->changeProfile(_profile , map , false);
}
void EditProfileDialog::previewColorScheme(const QModelIndex& index)
{
    const QString& name = index.data(Qt::UserRole + 1).value<const ColorScheme*>()->name();

    delayedPreview(Profile::ColorScheme , name);
}
void EditProfileDialog::removeColorScheme()
{
    QModelIndexList selected = _ui->colorSchemeList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString& name = selected.first().data(Qt::UserRole + 1).value<const ColorScheme*>()->name();

        if (ColorSchemeManager::instance()->deleteColorScheme(name))
            _ui->colorSchemeList->model()->removeRow(selected.first().row());
    }
}
void EditProfileDialog::showColorSchemeEditor(bool isNewScheme)
{
    QModelIndexList selected = _ui->colorSchemeList->selectionModel()->selectedIndexes();

    QAbstractItemModel* model = _ui->colorSchemeList->model();
    const ColorScheme* colors = 0;
    if (!selected.isEmpty())
        colors = model->data(selected.first(), Qt::UserRole + 1).value<const ColorScheme*>();
    else
        colors = ColorSchemeManager::instance()->defaultColorScheme();

    Q_ASSERT(colors);

    QWeakPointer<KDialog> dialog = new KDialog(this);

    if (isNewScheme)
        dialog.data()->setCaption(i18n("New Color Scheme"));
    else
        dialog.data()->setCaption(i18n("Edit Color Scheme"));

    ColorSchemeEditor* editor = new ColorSchemeEditor;
    dialog.data()->setMainWidget(editor);
    editor->setup(colors);

    if (isNewScheme)
        editor->setDescription(i18n("New Color Scheme"));

    if (dialog.data()->exec() == QDialog::Accepted) {
        ColorScheme* newScheme = new ColorScheme(*editor->colorScheme());

        // if this is a new color scheme, pick a name based on the description
        if (isNewScheme)
            newScheme->setName(newScheme->description());

        ColorSchemeManager::instance()->addColorScheme(newScheme);

        updateColorSchemeList(true);

        preview(Profile::ColorScheme, newScheme->name());
    }
    delete dialog.data();
}
void EditProfileDialog::newColorScheme()
{
    showColorSchemeEditor(true);
}
void EditProfileDialog::editColorScheme()
{
    showColorSchemeEditor(false);
}
void EditProfileDialog::colorSchemeSelected()
{
    QModelIndexList selected = _ui->colorSchemeList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        QAbstractItemModel* model = _ui->colorSchemeList->model();
        const ColorScheme* colors = model->data(selected.first(), Qt::UserRole + 1).value<const ColorScheme*>();

        updateTempProfileProperty(Profile::ColorScheme, colors->name());
        previewColorScheme(selected.first());

        updateTransparencyWarning();
    }

    updateColorSchemeButtons();
}
void EditProfileDialog::updateColorSchemeButtons()
{
    enableIfNonEmptySelection(_ui->editColorSchemeButton, _ui->colorSchemeList->selectionModel());
    enableIfNonEmptySelection(_ui->removeColorSchemeButton, _ui->colorSchemeList->selectionModel());
}
void EditProfileDialog::updateKeyBindingsButtons()
{
    enableIfNonEmptySelection(_ui->editKeyBindingsButton, _ui->keyBindingList->selectionModel());
    enableIfNonEmptySelection(_ui->removeKeyBindingsButton, _ui->keyBindingList->selectionModel());
}
void EditProfileDialog::enableIfNonEmptySelection(QWidget* widget, QItemSelectionModel* selectionModel)
{
    widget->setEnabled(selectionModel->hasSelection());
}
void EditProfileDialog::updateTransparencyWarning()
{
    // zero or one indexes can be selected
    foreach(const QModelIndex & index , _ui->colorSchemeList->selectionModel()->selectedIndexes()) {
        bool needTransparency = index.data(Qt::UserRole + 1).value<const ColorScheme*>()->opacity() < 1.0;

        if (!needTransparency) {
            _ui->transparencyWarningWidget->setHidden(true);
        } else if (!KWindowSystem::compositingActive()) {
            _ui->transparencyWarningWidget->setText(i18n("This color scheme uses a transparent background"
                                                    " which does not appear to be supported on your"
                                                    " desktop"));
            _ui->transparencyWarningWidget->setHidden(false);
        } else if (!WindowSystemInfo::HAVE_TRANSPARENCY) {
            _ui->transparencyWarningWidget->setText(i18n("Konsole was started before desktop effects were enabled."
                                                    " You need to restart Konsole to see transparent background."));
            _ui->transparencyWarningWidget->setHidden(false);
        }
    }
}

void EditProfileDialog::createTempProfile()
{
    _tempProfile = Profile::Ptr(new Profile);
    _tempProfile->setHidden(true);
}

void EditProfileDialog::updateTempProfileProperty(Profile::Property aProperty, const QVariant & value)
{
    _tempProfile->setProperty(aProperty, value);
    updateButtonApply();
}

void EditProfileDialog::updateButtonApply()
{
    bool userModified = false;

    QHashIterator<Profile::Property, QVariant> iter(_tempProfile->setProperties());
    while (iter.hasNext()) {
        iter.next();

        Profile::Property aProperty = iter.key();
        QVariant value = iter.value();

        // for previewed property
        if (_previewedProperties.contains(int(aProperty))) {
            if (value != _previewedProperties.value(int(aProperty))) {
                userModified = true;
                break;
            }
            // for not-previewed property
        } else if ((value != _profile->property<QVariant>(aProperty))) {
            userModified = true;
            break;
        }
    }

    enableButtonApply(userModified);
}

void EditProfileDialog::setupKeyboardPage(const Profile::Ptr /* profile */)
{
    // setup translator list
    updateKeyBindingsList(true);

    connect(_ui->keyBindingList->selectionModel(),
            SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(keyBindingSelected()));
    connect(_ui->newKeyBindingsButton, SIGNAL(clicked()), this,
            SLOT(newKeyBinding()));

    updateKeyBindingsButtons();

    connect(_ui->editKeyBindingsButton, SIGNAL(clicked()), this,
            SLOT(editKeyBinding()));
    connect(_ui->removeKeyBindingsButton, SIGNAL(clicked()), this,
            SLOT(removeKeyBinding()));
}
void EditProfileDialog::keyBindingSelected()
{
    QModelIndexList selected = _ui->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        QAbstractItemModel* model = _ui->keyBindingList->model();
        const KeyboardTranslator* translator = model->data(selected.first(), Qt::UserRole + 1)
                                               .value<const KeyboardTranslator*>();
        updateTempProfileProperty(Profile::KeyBindings, translator->name());
    }

    updateKeyBindingsButtons();
}
void EditProfileDialog::removeKeyBinding()
{
    QModelIndexList selected = _ui->keyBindingList->selectionModel()->selectedIndexes();

    if (!selected.isEmpty()) {
        const QString& name = selected.first().data(Qt::UserRole + 1).value<const KeyboardTranslator*>()->name();
        if (KeyboardTranslatorManager::instance()->deleteTranslator(name))
            _ui->keyBindingList->model()->removeRow(selected.first().row());
    }
}
void EditProfileDialog::showKeyBindingEditor(bool isNewTranslator)
{
    QModelIndexList selected = _ui->keyBindingList->selectionModel()->selectedIndexes();
    QAbstractItemModel* model = _ui->keyBindingList->model();

    const KeyboardTranslator* translator = 0;
    if (!selected.isEmpty())
        translator = model->data(selected.first(), Qt::UserRole + 1).value<const KeyboardTranslator*>();
    else
        translator = KeyboardTranslatorManager::instance()->defaultTranslator();

    Q_ASSERT(translator);

    QWeakPointer<KDialog> dialog = new KDialog(this);

    if (isNewTranslator)
        dialog.data()->setCaption(i18n("New Key Binding List"));
    else
        dialog.data()->setCaption(i18n("Edit Key Binding List"));

    KeyBindingEditor* editor = new KeyBindingEditor;
    dialog.data()->setMainWidget(editor);

    if (translator)
        editor->setup(translator);

    if (isNewTranslator)
        editor->setDescription(i18n("New Key Binding List"));

    if (dialog.data()->exec() == QDialog::Accepted) {
        KeyboardTranslator* newTranslator = new KeyboardTranslator(*editor->translator());

        if (isNewTranslator)
            newTranslator->setName(newTranslator->description());

        KeyboardTranslatorManager::instance()->addTranslator(newTranslator);

        updateKeyBindingsList();

        const QString& currentTranslator = lookupProfile()
                                           ->property<QString>(Profile::KeyBindings);

        if (newTranslator->name() == currentTranslator) {
            updateTempProfileProperty(Profile::KeyBindings, newTranslator->name());
        }
    }
    delete dialog.data();
}
void EditProfileDialog::newKeyBinding()
{
    showKeyBindingEditor(true);
}
void EditProfileDialog::editKeyBinding()
{
    showKeyBindingEditor(false);
}
void EditProfileDialog::setupCheckBoxes(BooleanOption* options , const Profile::Ptr profile)
{
    while (options->button != 0) {
        options->button->setChecked(profile->property<bool>(options->property));
        connect(options->button, SIGNAL(toggled(bool)), this, options->slot);

        ++options;
    }
}
void EditProfileDialog::setupRadio(RadioOption* possibilities , int actual)
{
    while (possibilities->button != 0) {
        if (possibilities->value == actual)
            possibilities->button->setChecked(true);
        else
            possibilities->button->setChecked(false);

        connect(possibilities->button, SIGNAL(clicked()), this, possibilities->slot);

        ++possibilities;
    }
}

void EditProfileDialog::setupScrollingPage(const Profile::Ptr profile)
{
    // setup scrollbar radio
    int scrollBarPosition = profile->property<int>(Profile::ScrollBarPosition);

    RadioOption positions[] = { {_ui->scrollBarHiddenButton, Enum::ScrollBarHidden, SLOT(hideScrollBar())},
        {_ui->scrollBarLeftButton, Enum::ScrollBarLeft, SLOT(showScrollBarLeft())},
        {_ui->scrollBarRightButton, Enum::ScrollBarRight, SLOT(showScrollBarRight())},
        {0, 0, 0}
    };

    setupRadio(positions , scrollBarPosition);

    // setup scrollback type radio
    int scrollBackType = profile->property<int>(Profile::HistoryMode);
    _ui->historySizeWidget->setMode(Enum::HistoryModeEnum(scrollBackType));
    connect(_ui->historySizeWidget, SIGNAL(historyModeChanged(Enum::HistoryModeEnum)),
            this, SLOT(historyModeChanged(Enum::HistoryModeEnum)));

    // setup scrollback line count spinner
    const int historySize = profile->historySize();
    _ui->historySizeWidget->setLineCount(historySize);

    // signals and slots
    connect(_ui->historySizeWidget, SIGNAL(historySizeChanged(int)),
            this, SLOT(historySizeChanged(int)));
}

void EditProfileDialog::historySizeChanged(int lineCount)
{
    updateTempProfileProperty(Profile::HistorySize , lineCount);
}
void EditProfileDialog::historyModeChanged(Enum::HistoryModeEnum mode)
{
    updateTempProfileProperty(Profile::HistoryMode, mode);
}
void EditProfileDialog::hideScrollBar()
{
    updateTempProfileProperty(Profile::ScrollBarPosition, Enum::ScrollBarHidden);
}
void EditProfileDialog::showScrollBarLeft()
{
    updateTempProfileProperty(Profile::ScrollBarPosition, Enum::ScrollBarLeft);
}
void EditProfileDialog::showScrollBarRight()
{
    updateTempProfileProperty(Profile::ScrollBarPosition, Enum::ScrollBarRight);
}
void EditProfileDialog::setupMousePage(const Profile::Ptr profile)
{
    BooleanOption  options[] = { {
            _ui->underlineLinksButton , Profile::UnderlineLinksEnabled,
            SLOT(toggleUnderlineLinks(bool))
        },
        {
            _ui->ctrlRequiredForDragButton, Profile::CtrlRequiredForDrag,
            SLOT(toggleCtrlRequiredForDrag(bool))
        },
        {
            _ui->copyTextToClipboardButton , Profile::AutoCopySelectedText,
            SLOT(toggleCopyTextToClipboard(bool))
        },
        {
            _ui->trimTrailingSpacesButton , Profile::TrimTrailingSpacesInSelectedText,
            SLOT(toggleTrimTrailingSpacesInSelectedText(bool))
        },
        {
            _ui->openLinksByDirectClickButton , Profile::OpenLinksByDirectClickEnabled,
            SLOT(toggleOpenLinksByDirectClick(bool))
        },
        { 0 , Profile::Property(0) , 0 }
    };
    setupCheckBoxes(options , profile);

    // setup middle click paste mode
    const int middleClickPasteMode = profile->property<int>(Profile::MiddleClickPasteMode);
    RadioOption pasteModes[] = {
        {_ui->pasteFromX11SelectionButton, Enum::PasteFromX11Selection, SLOT(pasteFromX11Selection())},
        {_ui->pasteFromClipboardButton, Enum::PasteFromClipboard, SLOT(pasteFromClipboard())},
        {0, 0, 0}
    };
    setupRadio(pasteModes , middleClickPasteMode);

    // interaction options
    _ui->wordCharacterEdit->setText(profile->wordCharacters());

    connect(_ui->wordCharacterEdit, SIGNAL(textChanged(QString)), this,
            SLOT(wordCharactersChanged(QString)));

    int tripleClickMode = profile->property<int>(Profile::TripleClickMode);
    _ui->tripleClickModeCombo->setCurrentIndex(tripleClickMode);

    connect(_ui->tripleClickModeCombo, SIGNAL(activated(int)), this,
            SLOT(TripleClickModeChanged(int)));

    _ui->openLinksByDirectClickButton->setEnabled(_ui->underlineLinksButton->isChecked());
}
void EditProfileDialog::setupAdvancedPage(const Profile::Ptr profile)
{
    BooleanOption  options[] = { {
            _ui->enableBlinkingTextButton , Profile::BlinkingTextEnabled ,
            SLOT(toggleBlinkingText(bool))
        },
        {
            _ui->enableFlowControlButton , Profile::FlowControlEnabled ,
            SLOT(toggleFlowControl(bool))
        },
        {
            _ui->enableBlinkingCursorButton , Profile::BlinkingCursorEnabled ,
            SLOT(toggleBlinkingCursor(bool))
        },
        {
            _ui->enableBidiRenderingButton , Profile::BidiRenderingEnabled ,
            SLOT(togglebidiRendering(bool))
        },
        { 0 , Profile::Property(0) , 0 }
    };
    setupCheckBoxes(options , profile);

    const int lineSpacing = profile->lineSpacing();
    _ui->lineSpacingSpinner->setValue(lineSpacing);

    connect(_ui->lineSpacingSpinner, SIGNAL(valueChanged(int)),
            this, SLOT(lineSpacingChanged(int)));

    // cursor options
    if (profile->useCustomCursorColor())
        _ui->customCursorColorButton->setChecked(true);
    else
        _ui->autoCursorColorButton->setChecked(true);

    _ui->customColorSelectButton->setColor(profile->customCursorColor());

    connect(_ui->customCursorColorButton, SIGNAL(clicked()), this, SLOT(customCursorColor()));
    connect(_ui->autoCursorColorButton, SIGNAL(clicked()), this, SLOT(autoCursorColor()));
    connect(_ui->customColorSelectButton, SIGNAL(changed(QColor)),
            SLOT(customCursorColorChanged(QColor)));

    int shape = profile->property<int>(Profile::CursorShape);
    _ui->cursorShapeCombo->setCurrentIndex(shape);

    connect(_ui->cursorShapeCombo, SIGNAL(activated(int)), this, SLOT(setCursorShape(int)));

    // encoding options
    QAction* codecAction = new KCodecAction(this);
    _ui->selectEncodingButton->setMenu(codecAction->menu());
    connect(codecAction, SIGNAL(triggered(QTextCodec*)), this, SLOT(setDefaultCodec(QTextCodec*)));

    _ui->characterEncodingLabel->setText(profile->defaultEncoding());
}
void EditProfileDialog::setDefaultCodec(QTextCodec* codec)
{
    QString name = QString(codec->name());

    updateTempProfileProperty(Profile::DefaultEncoding, name);
    _ui->characterEncodingLabel->setText(codec->name());
}
void EditProfileDialog::customCursorColorChanged(const QColor& color)
{
    updateTempProfileProperty(Profile::CustomCursorColor, color);

    // ensure that custom cursor colors are enabled
    _ui->customCursorColorButton->click();
}
void EditProfileDialog::wordCharactersChanged(const QString& text)
{
    updateTempProfileProperty(Profile::WordCharacters, text);
}
void EditProfileDialog::autoCursorColor()
{
    updateTempProfileProperty(Profile::UseCustomCursorColor, false);
}
void EditProfileDialog::customCursorColor()
{
    updateTempProfileProperty(Profile::UseCustomCursorColor, true);
}
void EditProfileDialog::setCursorShape(int index)
{
    updateTempProfileProperty(Profile::CursorShape, index);
}
void EditProfileDialog::togglebidiRendering(bool enable)
{
    updateTempProfileProperty(Profile::BidiRenderingEnabled, enable);
}
void EditProfileDialog::lineSpacingChanged(int spacing)
{
    updateTempProfileProperty(Profile::LineSpacing, spacing);
}
void EditProfileDialog::toggleBlinkingCursor(bool enable)
{
    updateTempProfileProperty(Profile::BlinkingCursorEnabled, enable);
}
void EditProfileDialog::toggleUnderlineLinks(bool enable)
{
    updateTempProfileProperty(Profile::UnderlineLinksEnabled, enable);
    _ui->openLinksByDirectClickButton->setEnabled(enable);
}
void EditProfileDialog::toggleCtrlRequiredForDrag(bool enable)
{
    updateTempProfileProperty(Profile::CtrlRequiredForDrag, enable);
}
void EditProfileDialog::toggleOpenLinksByDirectClick(bool enable)
{
    updateTempProfileProperty(Profile::OpenLinksByDirectClickEnabled, enable);
}
void EditProfileDialog::toggleCopyTextToClipboard(bool enable)
{
    updateTempProfileProperty(Profile::AutoCopySelectedText, enable);
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
void EditProfileDialog::toggleBlinkingText(bool enable)
{
    updateTempProfileProperty(Profile::BlinkingTextEnabled, enable);
}
void EditProfileDialog::toggleFlowControl(bool enable)
{
    updateTempProfileProperty(Profile::FlowControlEnabled, enable);
}
void EditProfileDialog::fontSelected(const QFont& aFont)
{
    QFont previewFont = aFont;

    setFontInputValue(aFont);

    _ui->fontPreviewLabel->setFont(previewFont);

    preview(Profile::Font, aFont);
    updateTempProfileProperty(Profile::Font, aFont);
}
void EditProfileDialog::showFontDialog()
{
    QString sampleText = QString("ell 'lL', one '1', little eye 'i', big eye");
    sampleText += QString("'I', lL1iI, Zero '0', little oh 'o', big oh 'O', 0oO");
    sampleText += QString("`~!@#$%^&*()_+-=[]\\{}|:\";'<>?,./");
    sampleText += QString("0123456789");
    sampleText += QString("\nThe Quick Brown Fox Jumps Over The Lazy Dog\n");
    sampleText += i18n("--- Type anything in this box ---");
    QFont currentFont = _ui->fontPreviewLabel->font();

    QWeakPointer<KFontDialog> dialog = new KFontDialog(this, KFontChooser::FixedFontsOnly);
    dialog.data()->setCaption(i18n("Select Fixed Width Font"));
    dialog.data()->setFont(currentFont, true);

    // TODO (hindenburg): When https://git.reviewboard.kde.org/r/103357 is
    // committed, change the below.
    // Use text more fitting to show font differences in a terminal
    QList<KFontChooser*> chooserList = dialog.data()->findChildren<KFontChooser*>();
    if (!chooserList.isEmpty())
        chooserList.at(0)->setSampleText(sampleText);

    connect(dialog.data(), SIGNAL(fontSelected(QFont)), this, SLOT(fontSelected(QFont)));

    if (dialog.data()->exec() == QDialog::Rejected)
        fontSelected(currentFont);
    delete dialog.data();
}
void EditProfileDialog::setFontSize(double pointSize)
{
    QFont newFont = _ui->fontPreviewLabel->font();
    newFont.setPointSizeF(pointSize);
    _ui->fontPreviewLabel->setFont(newFont);

    preview(Profile::Font, newFont);
    updateTempProfileProperty(Profile::Font, newFont);
}

void EditProfileDialog::setFontInputValue(const QFont& aFont)
{
    _ui->fontSizeInput->setValue(aFont.pointSizeF());
}

ColorSchemeViewDelegate::ColorSchemeViewDelegate(QObject* aParent)
    : QAbstractItemDelegate(aParent)
{
}

void ColorSchemeViewDelegate::setEntryTimeLine(QTimeLine* timeLine)
{
    _entryTimeLine = timeLine;
}

void ColorSchemeViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    // entry animation
    //
    // note that the translation occurs for each item drawn, but the
    // painter is not reset between painting items.  this means that when
    // the items are painted in order ( as occurs when the list is first
    // shown ), there is a visually pleasing staggering of items as they
    // enter.
    if (_entryTimeLine != 0) {
        qreal value = 1.0 - _entryTimeLine->currentValue();
        painter->translate(value *
                           option.rect.width() , 0);

        painter->setOpacity(_entryTimeLine->currentValue());
    }

    const ColorScheme* scheme = index.data(Qt::UserRole + 1).value<const ColorScheme*>();

    Q_ASSERT(scheme);

    bool transparencyAvailable = KWindowSystem::compositingActive();

    painter->setRenderHint(QPainter::Antialiasing);

    // draw background
    painter->setPen(QPen(scheme->foregroundColor() , 1));

    // radial gradient for background
    // from a lightened version of the scheme's background color in the center to
    // a darker version at the outer edge
    QColor color = scheme->backgroundColor();
    QRectF backgroundRect = QRectF(option.rect).adjusted(1.5, 1.5, -1.5, -1.5);

    QRadialGradient backgroundGradient(backgroundRect.center() , backgroundRect.width() / 2);
    backgroundGradient.setColorAt(0 , color.lighter(105));
    backgroundGradient.setColorAt(1 , color.darker(115));

    const int backgroundRectXRoundness = 4;
    const int backgroundRectYRoundness = 30;

    QPainterPath backgroundRectPath(backgroundRect.topLeft());
    backgroundRectPath.addRoundRect(backgroundRect , backgroundRectXRoundness , backgroundRectYRoundness);

    if (transparencyAvailable) {
        painter->save();
        color.setAlphaF(scheme->opacity());
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setBrush(backgroundGradient);

        painter->drawPath(backgroundRectPath);
        painter->restore();
    } else {
        painter->setBrush(backgroundGradient);
        painter->drawPath(backgroundRectPath);
    }

    // draw stripe at the side using scheme's foreground color
    painter->setPen(QPen(Qt::NoPen));
    QPainterPath path(option.rect.topLeft());
    path.lineTo(option.rect.width() / 10.0 , option.rect.top());
    path.lineTo(option.rect.bottomLeft());
    path.lineTo(option.rect.topLeft());
    painter->setBrush(scheme->foregroundColor());
    painter->drawPath(path.intersected(backgroundRectPath));

    // draw highlight
    // with a linear gradient going from translucent white to transparent
    QLinearGradient gradient(option.rect.topLeft() , option.rect.bottomLeft());
    gradient.setColorAt(0 , QColor(255, 255, 255, 90));
    gradient.setColorAt(1 , Qt::transparent);
    painter->setBrush(gradient);
    painter->drawRoundRect(backgroundRect , 4 , 30);

    //const bool isChecked = index.data(Qt::CheckStateRole) == Qt::Checked;
    const bool isSelected = option.state & QStyle::State_Selected;

    // draw border on selected items
    if (isSelected) { //|| isChecked )
        static const int selectedBorderWidth = 6;

        painter->setBrush(QBrush(Qt::NoBrush));
        QPen pen;

        QColor highlightColor = option.palette.highlight().color();

        if (isSelected)
            highlightColor.setAlphaF(1.0);
        else
            highlightColor.setAlphaF(0.7);

        pen.setBrush(highlightColor);
        pen.setWidth(selectedBorderWidth);
        pen.setJoinStyle(Qt::MiterJoin);

        painter->setPen(pen);

        painter->drawRect(option.rect.adjusted(selectedBorderWidth / 2,
                                               selectedBorderWidth / 2,
                                               -selectedBorderWidth / 2,
                                               -selectedBorderWidth / 2));
    }

    // draw color scheme name using scheme's foreground color
    QPen pen(scheme->foregroundColor());
    painter->setPen(pen);

    painter->drawText(option.rect , Qt::AlignCenter ,
                      index.data(Qt::DisplayRole).value<QString>());
}

QSize ColorSchemeViewDelegate::sizeHint(const QStyleOptionViewItem& option,
                                        const QModelIndex& /*index*/) const
{
    const int width = 200;
    qreal colorWidth = (qreal)width / TABLE_COLORS;
    int margin = 5;
    qreal heightForWidth = (colorWidth * 2) + option.fontMetrics.height() + margin;

    // temporary
    return QSize(width, (int)heightForWidth);
}

#include "EditProfileDialog.moc"
