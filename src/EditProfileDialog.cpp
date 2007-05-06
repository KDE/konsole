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
#include <QHideEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QStandardItemModel>
#include <QtDebug>

// KDE
#include <KFontDialog>
#include <KIcon>
#include <KIconDialog>
#include <KDirSelectDialog>
#include <KUrlCompletion>

// Konsole
#include "ColorScheme.h"
#include "ColorSchemeEditor.h"
#include "ui_EditProfileDialog.h"
#include "EditProfileDialog.h"
#include "EditTabTitleFormatDialog.h"
#include "KeyBindingEditor.h"
#include "KeyboardTranslator.h"
#include "SessionManager.h"
#include "ShellCommand.h"

using namespace Konsole;

EditProfileDialog::EditProfileDialog(QWidget* parent)
    : KDialog(parent)
{
    setCaption("Edit Profile");
    setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply | KDialog::Default );

    connect( this , SIGNAL(applyClicked()) , this , SLOT(save()) );

    _ui = new Ui::EditProfileDialog();
    _ui->setupUi(mainWidget());

    _tempProfile = new Profile;
}
EditProfileDialog::~EditProfileDialog()
{
    delete _ui;
    delete _tempProfile;
}
void EditProfileDialog::save()
{
    if ( _tempProfile->isEmpty() )
        return;

    SessionManager::instance()->changeProfile(_profileKey,_tempProfile->setProperties());

    // ensure that these settings are not undone by a call
    // to unpreview()
    QHashIterator<Profile::Property,QVariant> iter(_tempProfile->setProperties());
    while ( iter.hasNext() )
    {
        iter.next();
        _previewedProperties.remove(iter.key());
    }
}
void EditProfileDialog::reject()
{
    unpreviewAll();
    KDialog::reject();
}
void EditProfileDialog::accept()
{
    save();
    unpreviewAll();
    KDialog::accept(); 
}
void EditProfileDialog::setProfile(const QString& key)
{
    _profileKey = key;

    const Profile* info = SessionManager::instance()->profile(key);

    Q_ASSERT( info );

    // update caption
    setCaption( QString("Edit Profile \"%1\"").arg(info->name()) );

    // setup each page of the dialog
    setupGeneralPage(info);
    setupAppearencePage(info);
    setupKeyboardPage(info);
    setupScrollingPage(info);
    setupAdvancedPage(info);

    if ( _tempProfile )
    {
        delete _tempProfile;
        _tempProfile = new Profile;
    }
}
void EditProfileDialog::setupGeneralPage(const Profile* info)
{
    _ui->profileNameEdit->setText( info->name() );

    ShellCommand command( info->command() , info->arguments() );
    _ui->commandEdit->setText( command.fullCommand() );

    KUrlCompletion* exeCompletion = new KUrlCompletion(KUrlCompletion::ExeCompletion);
    exeCompletion->setDir(QString::null);
    _ui->commandEdit->setCompletionObject( exeCompletion );
    _ui->initialDirEdit->setText( info->defaultWorkingDirectory() );
    _ui->initialDirEdit->setCompletionObject( new KUrlCompletion(KUrlCompletion::DirCompletion) );
    _ui->initialDirEdit->setClearButtonShown(true);
    _ui->iconSelectButton->setIcon( KIcon(info->icon()) );

    _ui->tabTitleEdit->setText( info->property(Profile::LocalTabTitleFormat).value<QString>() );
    _ui->remoteTabTitleEdit->setText( 
            info->property(Profile::RemoteTabTitleFormat).value<QString>());

    // tab mode
    int tabMode = info->property(Profile::TabBarMode).value<int>();

    RadioOption tabModes[] = { {_ui->alwaysHideTabBarButton,Profile::AlwaysHideTabBar,SLOT(alwaysHideTabBar())},
                            {_ui->alwaysShowTabBarButton,Profile::AlwaysShowTabBar,SLOT(alwaysShowTabBar())},
                            {_ui->autoShowTabBarButton,Profile::ShowTabBarAsNeeded,SLOT(showTabBarAsNeeded())},
                            {0,0,0} };
    setupRadio( tabModes , tabMode );

    _ui->showMenuBarButton->setChecked( info->property(Profile::ShowMenuBar).value<bool>() );

    // signals and slots
    connect( _ui->dirSelectButton , SIGNAL(clicked()) , this , SLOT(selectInitialDir()) );
    connect( _ui->iconSelectButton , SIGNAL(clicked()) , this , SLOT(selectIcon()) );

    connect( _ui->profileNameEdit , SIGNAL(textChanged(const QString&)) , this ,
            SLOT(profileNameChanged(const QString&)) );
    connect( _ui->initialDirEdit , SIGNAL(textChanged(const QString&)) , this , 
            SLOT(initialDirChanged(const QString&)) );
    connect(_ui->commandEdit , SIGNAL(textChanged(const QString&)) , this ,
            SLOT(commandChanged(const QString&)) ); 
    
    connect(_ui->tabTitleEdit , SIGNAL(textChanged(const QString&)) , this ,
            SLOT(tabTitleFormatChanged(const QString&)) );
    connect(_ui->remoteTabTitleEdit , SIGNAL(textChanged(const QString&)) , this ,
            SLOT(remoteTabTitleFormatChanged(const QString&)));
    connect(_ui->tabTitleEditButton , SIGNAL(clicked()) , this , 
            SLOT(editTabTitle()) );
    connect(_ui->remoteTabTitleEditButton , SIGNAL(clicked()) , this ,
            SLOT(editRemoteTabTitle()) );

    connect(_ui->showMenuBarButton , SIGNAL(toggled(bool)) , this , 
            SLOT(showMenuBar(bool)) );
}
void EditProfileDialog::showMenuBar(bool show)
{
    _tempProfile->setProperty(Profile::ShowMenuBar,show);
}
void EditProfileDialog::alwaysHideTabBar()
{
    _tempProfile->setProperty(Profile::TabBarMode,Profile::AlwaysHideTabBar);
}
void EditProfileDialog::alwaysShowTabBar()
{
    _tempProfile->setProperty(Profile::TabBarMode,Profile::AlwaysShowTabBar);
}
void EditProfileDialog::showTabBarAsNeeded()
{
    _tempProfile->setProperty(Profile::TabBarMode,Profile::ShowTabBarAsNeeded);
}
void EditProfileDialog::editTabTitle()
{
    EditTabTitleFormatDialog dialog(this);
    dialog.setContext(Session::LocalTabTitle);
    dialog.setTabTitleFormat(_ui->tabTitleEdit->text());
  
    if ( dialog.exec() == QDialog::Accepted )
    {
        _ui->tabTitleEdit->setText(dialog.tabTitleFormat());
    }
}
void EditProfileDialog::editRemoteTabTitle()
{
    EditTabTitleFormatDialog dialog(this);
    dialog.setContext(Session::RemoteTabTitle);
    dialog.setTabTitleFormat(_ui->remoteTabTitleEdit->text());
    
    if ( dialog.exec() == QDialog::Accepted )
    {
        _ui->remoteTabTitleEdit->setText(dialog.tabTitleFormat());
    }
}
void EditProfileDialog::tabTitleFormatChanged(const QString& format)
{
    _tempProfile->setProperty(Profile::LocalTabTitleFormat,format);
}
void EditProfileDialog::remoteTabTitleFormatChanged(const QString& format)
{
    _tempProfile->setProperty(Profile::RemoteTabTitleFormat,format);
}

void EditProfileDialog::selectIcon()
{
    const QString& icon = KIconDialog::getIcon();
    if (!icon.isEmpty())
    {
        _ui->iconSelectButton->setIcon( KIcon(icon) );
        _tempProfile->setProperty(Profile::Icon,icon);
    }
}
void EditProfileDialog::profileNameChanged(const QString& text)
{
    _tempProfile->setProperty(Profile::Name,text);
}
void EditProfileDialog::initialDirChanged(const QString& dir)
{
    _tempProfile->setProperty(Profile::Directory,dir);
}
void EditProfileDialog::commandChanged(const QString& command)
{
    ShellCommand shellCommand(command);

    //TODO Split into command and arguments
    _tempProfile->setProperty(Profile::Command,shellCommand.command());
    _tempProfile->setProperty(Profile::Arguments,shellCommand.arguments());
}
void EditProfileDialog::selectInitialDir()
{
    const KUrl& url = KDirSelectDialog::selectDirectory(_ui->initialDirEdit->text(),
                                                        true,
                                                        0L,
                                                        i18n("Select Initial Directory"));

    if ( !url.isEmpty() )
        _ui->initialDirEdit->setText(url.path());
}
void EditProfileDialog::setupAppearencePage(const Profile* info)
{
    // setup color list
    updateColorSchemeList();

    _ui->colorSchemeList->setItemDelegate(new ColorSchemeViewDelegate(this));
    _ui->colorSchemeList->setMouseTracking(true);
    _ui->colorSchemeList->installEventFilter(this);

    connect( _ui->colorSchemeList , SIGNAL(doubleClicked(const QModelIndex&)) , this ,
            SLOT(colorSchemeSelected()) );
    connect( _ui->colorSchemeList , SIGNAL(entered(const QModelIndex&)) , this , 
            SLOT(previewColorScheme(const QModelIndex&)) );

    connect( _ui->selectColorSchemeButton , SIGNAL(clicked()) , this , 
            SLOT(colorSchemeSelected()) );
    connect( _ui->editColorSchemeButton , SIGNAL(clicked()) , this , 
            SLOT(editColorScheme()) );
    connect( _ui->removeColorSchemeButton , SIGNAL(clicked()) , this ,
            SLOT(removeColorScheme()) );
    connect( _ui->newColorSchemeButton , SIGNAL(clicked()) , this , 
            SLOT(newColorScheme()) );

    // setup font preview
    const QFont& font = info->font();
    _ui->fontPreviewLabel->setFont( font );
    _ui->fontSizeSlider->setValue( font.pointSize() );

    connect( _ui->fontSizeSlider , SIGNAL(valueChanged(int)) , this ,
             SLOT(setFontSize(int)) );
    connect( _ui->editFontButton , SIGNAL(clicked()) , this ,
             SLOT(showFontDialog()) );
}
void EditProfileDialog::updateColorSchemeList()
{
    delete _ui->colorSchemeList->model();

    const QString& name = SessionManager::instance()->profile(_profileKey)->colorScheme();
    const ColorScheme* currentScheme = ColorSchemeManager::instance()->findColorScheme(name);

    QStandardItemModel* model = new QStandardItemModel(this);
    QList<const ColorScheme*> schemeList = ColorSchemeManager::instance()->allColorSchemes();
    QListIterator<const ColorScheme*> schemeIter(schemeList);

    while (schemeIter.hasNext())
    {
        const ColorScheme* colors = schemeIter.next();
        QStandardItem* item = new QStandardItem(colors->description());
        item->setData( QVariant::fromValue(colors) ,  Qt::UserRole + 1);
        item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
        
        if ( colors == currentScheme )
           item->setCheckState( Qt::Checked );
        else 
            item->setCheckState( Qt::Unchecked );

        model->appendRow(item);
    }

    model->sort(0);

    _ui->colorSchemeList->setModel(model);

}
void EditProfileDialog::updateKeyBindingsList()
{
    delete _ui->keyBindingList->model();

    const QString& name = SessionManager::instance()->profile(_profileKey)->property(Profile::KeyBindings)
                                .value<QString>();

    const KeyboardTranslator* currentTranslator = KeyboardTranslatorManager::instance()->findTranslator(name);
    
    qDebug() << "Current translator = " << currentTranslator << ", name: " << name;

    QStandardItemModel* model = new QStandardItemModel(this);

    QList<QString> translatorNames = KeyboardTranslatorManager::instance()->allTranslators();
    QListIterator<QString> iter(translatorNames);
    while (iter.hasNext())
    {
        const QString& name = iter.next();

        const KeyboardTranslator* translator = KeyboardTranslatorManager::instance()->findTranslator(name);

        qDebug() << "Translator:" << translator << ", name = " << translator->name() << "description = " << 
            translator->description();

        // TODO Use translator->description() here
        QStandardItem* item = new QStandardItem(translator->description());
        item->setData(QVariant::fromValue(translator),Qt::UserRole+1);
        item->setFlags( item->flags() | Qt::ItemIsUserCheckable );

        if ( translator == currentTranslator )
            item->setCheckState( Qt::Checked );
        else
            item->setCheckState( Qt::Unchecked );

        model->appendRow(item);
    }

    model->sort(0);

    _ui->keyBindingList->setModel(model);
}
bool EditProfileDialog::eventFilter( QObject* watched , QEvent* event )
{
    if ( watched == _ui->colorSchemeList && event->type() == QEvent::Leave )
    {
        unpreview(Profile::ColorScheme);
    }

    return KDialog::eventFilter(watched,event);
}
void EditProfileDialog::unpreviewAll()
{
    qDebug() << "unpreviewing";

    QHash<Profile::Property,QVariant> map;
    QHashIterator<int,QVariant> iter(_previewedProperties);
    while ( iter.hasNext() )
    {
        iter.next();
        map.insert((Profile::Property)iter.key(),iter.value());
    }

    // undo any preview changes
    if ( !map.isEmpty() )
        SessionManager::instance()->changeProfile(_profileKey,map,false);
}
void EditProfileDialog::unpreview(int property)
{
    if (!_previewedProperties.contains(property))
        return;

    QHash<Profile::Property,QVariant> map;
    map.insert((Profile::Property)property,_previewedProperties[property]);
    SessionManager::instance()->changeProfile(_profileKey,map,false); 

    _previewedProperties.remove(property);
}
void EditProfileDialog::preview(int property , QVariant value)
{
    QHash<Profile::Property,QVariant> map;
    map.insert((Profile::Property)property,value);

    const Profile* original = SessionManager::instance()->profile(_profileKey);

    if (!_previewedProperties.contains(property))    
        _previewedProperties.insert(property , original->property((Profile::Property)property) );

    // temporary change to color scheme
    SessionManager::instance()->changeProfile( _profileKey , map , false);
}
void EditProfileDialog::previewColorScheme(const QModelIndex& index)
{
    const QString& name = index.data(Qt::UserRole+1).value<const ColorScheme*>()->name();

    preview( Profile::ColorScheme , name );
}
void EditProfileDialog::removeColorScheme()
{
    QModelIndexList selected = _ui->colorSchemeList->selectionModel()->selectedIndexes();

    if ( !selected.isEmpty() )
    {
        const QString& name = selected.first().data(Qt::UserRole+1).value<const ColorScheme*>()->name();
        ColorSchemeManager::instance()->deleteColorScheme(name);
        _ui->colorSchemeList->model()->removeRow(selected.first().row());
    }
}
void EditProfileDialog::showColorSchemeEditor(bool isNewScheme)
{    
    QModelIndexList selected = _ui->colorSchemeList->selectionModel()->selectedIndexes();

    QAbstractItemModel* model = _ui->colorSchemeList->model();
    QModelIndex index;
    if ( !selected.isEmpty() )
        index = selected.first();
    else
        index = model->index(0,0); // use the first item in the list

    const ColorScheme* colors = model->data(index,Qt::UserRole+1).value<const ColorScheme*>();

    KDialog* dialog = new KDialog(this);

    if ( isNewScheme )
        dialog->setCaption(i18n("New Color Scheme"));
    else
        dialog->setCaption(i18n("Edit Color Scheme"));

    ColorSchemeEditor* editor = new ColorSchemeEditor;
    dialog->setMainWidget(editor);
    editor->setup(colors);

    if ( isNewScheme )
        editor->setDescription(i18n("New Color Scheme"));
        
    if ( dialog->exec() == QDialog::Accepted )
    {
        ColorScheme* newScheme = new ColorScheme(*editor->colorScheme());

        // if this is a new color scheme, pick a name based on the description
        if ( isNewScheme )
            newScheme->setName(newScheme->description());

        ColorSchemeManager::instance()->addColorScheme( newScheme );
        
        updateColorSchemeList();

        const QString& currentScheme = SessionManager::instance()->profile(_profileKey)->colorScheme();

        // the next couple of lines may seem slightly odd,
        // but they force any open views based on the current profile
        // to update their color schemes
        if ( newScheme->name() == currentScheme )
        {
            _tempProfile->setProperty(Profile::ColorScheme,newScheme->name());
        }
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
void EditProfileDialog::colorSchemeSelected()
{
    QModelIndexList selected = _ui->colorSchemeList->selectionModel()->selectedIndexes();

    if ( !selected.isEmpty() )
    {
        QAbstractItemModel* model = _ui->colorSchemeList->model();
        const ColorScheme* colors = model->data(selected.first(),Qt::UserRole+1).value<const ColorScheme*>();

        _tempProfile->setProperty(Profile::ColorScheme,colors->name());

        changeCheckedItem(model,selected.first());
    }
}
void EditProfileDialog::setupKeyboardPage(const Profile* info)
{
    // setup translator list
 
    updateKeyBindingsList(); 

    connect( _ui->keyBindingList , SIGNAL(doubleClicked(const QModelIndex&)) , this , 
            SLOT(keyBindingSelected()) );
    connect( _ui->selectKeyBindingsButton , SIGNAL(clicked()) , this , 
            SLOT(keyBindingSelected()) );
    connect( _ui->newKeyBindingsButton , SIGNAL(clicked()) , this ,
            SLOT(newKeyBinding()) );
    connect( _ui->editKeyBindingsButton , SIGNAL(clicked()) , this , 
          SLOT(editKeyBinding()) );  
    connect( _ui->removeKeyBindingsButton , SIGNAL(clicked()) , this ,
            SLOT(removeKeyBinding()) );
}
void EditProfileDialog::keyBindingSelected()
{
    QModelIndexList selected = _ui->keyBindingList->selectionModel()->selectedIndexes();

    if ( !selected.isEmpty() )
    {
        QAbstractItemModel* model = _ui->keyBindingList->model();
        const KeyboardTranslator* translator = model->data(selected.first(),Qt::UserRole+1)
                                                .value<const KeyboardTranslator*>();
        _tempProfile->setProperty(Profile::KeyBindings,translator->name());

        changeCheckedItem(model,selected.first()); 
    }
}
void EditProfileDialog::changeCheckedItem( QAbstractItemModel* model , const QModelIndex& to )
{
        // uncheck current active item
        QModelIndexList list = model->match( model->index(0,0) , Qt::CheckStateRole , Qt::Checked );
        Q_ASSERT( list.count() == 1 );
        model->setData( list.first() , Qt::Unchecked , Qt::CheckStateRole ); 

        // check new active item
        model->setData( to , Qt::Checked , Qt::CheckStateRole );
}
void EditProfileDialog::removeKeyBinding()
{
    QModelIndexList selected = _ui->keyBindingList->selectionModel()->selectedIndexes();

    if ( !selected.isEmpty() )
    {
        const QString& name = selected.first().data(Qt::UserRole+1).value<const KeyboardTranslator*>()->name();
        KeyboardTranslatorManager::instance()->deleteTranslator(name);
        _ui->keyBindingList->model()->removeRow(selected.first().row());   
    }
}
void EditProfileDialog::showKeyBindingEditor(bool isNewTranslator)
{
    QModelIndexList selected = _ui->keyBindingList->selectionModel()->selectedIndexes();
    QAbstractItemModel* model = _ui->keyBindingList->model();

    QModelIndex index;
    if ( !selected.isEmpty() )
        index = selected.first();
    else
        index = model->index(0,0); // Use first item if there is no selection


    const KeyboardTranslator* translator = model->data(index,
                                            Qt::UserRole+1).value<const KeyboardTranslator*>();
    KDialog* dialog = new KDialog(this);

    if ( isNewTranslator )
        dialog->setCaption(i18n("New Key Binding List"));
    else
        dialog->setCaption(i18n("Edit Key Binding List"));

    KeyBindingEditor* editor = new KeyBindingEditor;
    dialog->setMainWidget(editor);
    editor->setup(translator);

    if ( isNewTranslator )
        editor->setDescription(i18n("New Key Binding List"));

    if ( dialog->exec() == QDialog::Accepted )
    {
        KeyboardTranslator* newTranslator = new KeyboardTranslator(*editor->translator());

        if ( isNewTranslator )
            newTranslator->setName(newTranslator->description());

        KeyboardTranslatorManager::instance()->addTranslator( newTranslator );

        updateKeyBindingsList();
        
        const QString& currentTranslator = SessionManager::instance()->profile(_profileKey)
                                        ->property(Profile::KeyBindings).value<QString>();

        if ( newTranslator->name() == currentTranslator )
        {
            _tempProfile->setProperty(Profile::KeyBindings,newTranslator->name());
        }
    }
}
void EditProfileDialog::newKeyBinding()
{
    showKeyBindingEditor(true);
}
void EditProfileDialog::editKeyBinding()
{
    showKeyBindingEditor(false);
}
void EditProfileDialog::setupCombo( ComboOption* options , const Profile* profile )
{
    while ( options->button != 0 )
    {
        options->button->setChecked( profile->property((Profile::Property)options->property).value<bool>() );
        connect( options->button , SIGNAL(toggled(bool)) , this , options->slot );

        ++options;
    }
}
void EditProfileDialog::setupRadio( RadioOption* possible , int actual )
{
    while (possible->button != 0)
    {
        if ( possible->property == actual )
            possible->button->setChecked(true);
        else
            possible->button->setChecked(false);
   
        connect( possible->button , SIGNAL(clicked()) , this , possible->slot );

        ++possible;
    }
}

void EditProfileDialog::setupScrollingPage(const Profile* profile)
{
    // setup scrollbar radio
    int scrollBarPosition = profile->property(Profile::ScrollBarPosition).value<int>();
   
    RadioOption positions[] = { {_ui->scrollBarHiddenButton,Profile::ScrollBarHidden,SLOT(hideScrollBar())},
                                {_ui->scrollBarLeftButton,Profile::ScrollBarLeft,SLOT(showScrollBarLeft())},
                                {_ui->scrollBarRightButton,Profile::ScrollBarRight,SLOT(showScrollBarRight())},
                                {0,0,0} 
                              }; 

    setupRadio( positions , scrollBarPosition );
   
    // setup scrollback type radio
    int scrollBackType = profile->property(Profile::HistoryMode).value<int>();
    
    RadioOption types[] = { {_ui->disableScrollbackButton,Profile::DisableHistory,SLOT(noScrollBack())},
                            {_ui->fixedScrollbackButton,Profile::FixedSizeHistory,SLOT(fixedScrollBack())},
                            {_ui->unlimitedScrollbackButton,Profile::UnlimitedHistory,SLOT(unlimitedScrollBack())},
                            {0,0,0} };
    setupRadio( types , scrollBackType ); 
    
    // setup scrollback line count spinner
    _ui->scrollBackLinesSpinner->setValue( profile->property(Profile::HistorySize).value<int>() );

    // signals and slots
    connect( _ui->scrollBackLinesSpinner , SIGNAL(valueChanged(int)) , this , 
            SLOT(scrollBackLinesChanged(int)) );
}

void EditProfileDialog::scrollBackLinesChanged(int lineCount)
{
    _tempProfile->setProperty(Profile::HistorySize , lineCount);
}
void EditProfileDialog::noScrollBack()
{
    _tempProfile->setProperty(Profile::HistoryMode , Profile::DisableHistory);
}
void EditProfileDialog::fixedScrollBack()
{
    _tempProfile->setProperty(Profile::HistoryMode , Profile::FixedSizeHistory);
}
void EditProfileDialog::unlimitedScrollBack()
{
    _tempProfile->setProperty(Profile::HistoryMode , Profile::UnlimitedHistory );
}
void EditProfileDialog::hideScrollBar()
{
    _tempProfile->setProperty(Profile::ScrollBarPosition , Profile::ScrollBarHidden );
}
void EditProfileDialog::showScrollBarLeft()
{
    _tempProfile->setProperty(Profile::ScrollBarPosition , Profile::ScrollBarLeft );
}
void EditProfileDialog::showScrollBarRight()
{
    _tempProfile->setProperty(Profile::ScrollBarPosition , Profile::ScrollBarRight );
}
void EditProfileDialog::setupAdvancedPage(const Profile* profile)
{
    ComboOption  options[] = { { _ui->enableBlinkingTextButton , Profile::BlinkingTextEnabled , 
                                 SLOT(toggleBlinkingText(bool)) },
                               { _ui->enableFlowControlButton , Profile::FlowControlEnabled ,
                                 SLOT(toggleFlowControl(bool)) },
                               { _ui->enableResizeWindowButton , Profile::AllowProgramsToResizeWindow ,
                                 SLOT(toggleResizeWindow(bool)) },
                               { _ui->enableBlinkingCursorButton , Profile::BlinkingCursorEnabled ,
                                 SLOT(toggleBlinkingCursor(bool)) },
                               { 0 , 0 , 0 }
                             };
    setupCombo( options , profile );

    // interaction options
    _ui->wordCharacterEdit->setText( profile->property(Profile::WordCharacters).value<QString>() );

    connect( _ui->wordCharacterEdit , SIGNAL(textChanged(const QString&)) , this , 
            SLOT(wordCharactersChanged(const QString&)) );

    // cursor options
    if ( profile->property(Profile::UseCustomCursorColor).value<bool>() )
        _ui->customCursorColorButton->setChecked(true);
    else
        _ui->autoCursorColorButton->setChecked(true);

    _ui->customColorSelectButton->setColor( profile->property(Profile::CustomCursorColor).value<QColor>() );

    connect( _ui->customCursorColorButton , SIGNAL(clicked()) , this , SLOT(customCursorColor()) );
    connect( _ui->autoCursorColorButton , SIGNAL(clicked()) , this , SLOT(autoCursorColor()) );
    connect( _ui->customColorSelectButton , SIGNAL(changed(const QColor&)) , 
            SLOT(customCursorColorChanged(const QColor&)) );

    int shape = profile->property(Profile::CursorShape).value<int>();
    _ui->cursorShapeCombo->setCurrentIndex(shape);

    connect( _ui->cursorShapeCombo , SIGNAL(activated(int)) , this , SLOT(setCursorShape(int)) ); 
}
void EditProfileDialog::customCursorColorChanged(const QColor& color)
{
    _tempProfile->setProperty(Profile::CustomCursorColor,color);

    // ensure that custom cursor colors are enabled
    _ui->customCursorColorButton->click();
}
void EditProfileDialog::wordCharactersChanged(const QString& text)
{
    _tempProfile->setProperty(Profile::WordCharacters,text);
}
void EditProfileDialog::autoCursorColor()
{
    _tempProfile->setProperty(Profile::UseCustomCursorColor,false);
}
void EditProfileDialog::customCursorColor()
{
    _tempProfile->setProperty(Profile::UseCustomCursorColor,true);
}
void EditProfileDialog::setCursorShape(int index)
{
    _tempProfile->setProperty(Profile::CursorShape,index);
}
void EditProfileDialog::toggleBlinkingCursor(bool enable)
{
    _tempProfile->setProperty(Profile::BlinkingCursorEnabled,enable);
}
void EditProfileDialog::toggleBlinkingText(bool enable)
{
    _tempProfile->setProperty(Profile::BlinkingTextEnabled,enable);
}
void EditProfileDialog::toggleFlowControl(bool enable)
{
    _tempProfile->setProperty(Profile::FlowControlEnabled,enable);
}
void EditProfileDialog::toggleResizeWindow(bool enable)
{
    _tempProfile->setProperty(Profile::AllowProgramsToResizeWindow,enable);
}
void EditProfileDialog::fontSelected(const QFont& font)
{
   qDebug() << "font selected";
   QSlider* slider = _ui->fontSizeSlider;
   
   _ui->fontSizeSlider->setRange( qMin(slider->minimum(),font.pointSize()) ,
                                  qMax(slider->maximum(),font.pointSize()) );
   _ui->fontSizeSlider->setValue(font.pointSize());
   _ui->fontPreviewLabel->setFont(font);

   _tempProfile->setProperty(Profile::Font,font);
   preview(Profile::Font,font);
}
void EditProfileDialog::showFontDialog()
{
    //TODO Only permit selection of mono-spaced fonts.  
    // the KFontDialog API does not appear to have a means to do this
    // at present.
    QFont currentFont = _ui->fontPreviewLabel->font();
   
    KFontDialog* dialog = new KFontDialog(this);
    dialog->setFont(currentFont);

    connect( dialog , SIGNAL(fontSelected(const QFont&)) , this , SLOT(fontSelected(const QFont&)) );
    dialog->show(); 
}
void EditProfileDialog::setFontSize(int pointSize)
{
    QFont newFont = _ui->fontPreviewLabel->font();
    newFont.setPointSize(pointSize);
    _ui->fontPreviewLabel->setFont( newFont );

    _tempProfile->setProperty(Profile::Font,newFont);

    preview(Profile::Font,newFont);
}
ColorSchemeViewDelegate::ColorSchemeViewDelegate(QObject* parent)
 : QAbstractItemDelegate(parent)
{

}

#if 0
QWidget* ColorSchemeViewDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, 
                                  const QModelIndex& index) const
{
    QWidget* holder = new QWidget(parent);
    QVBoxLayout* layout = new QVBoxLayout;

    QWidget* selectButton = new QPushButton(i18n("Use Color Scheme"));
    QWidget* editButton = new QPushButton(i18n("Edit..."));

    layout->setMargin(0);

    layout->addWidget(selectButton);
    layout->addWidget(editButton);

    holder->setLayout(layout);

    int width = holder->sizeHint().width();

    int left = option.rect.right() - width - 10;
    int top = option.rect.top();

    holder->move( left , top );

    return holder;
}
#endif

void ColorSchemeViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index) const
{
    const ColorScheme* scheme = index.data(Qt::UserRole + 1).value<const ColorScheme*>();

    Q_ASSERT(scheme);

    // draw background
    QBrush brush(scheme->backgroundColor());
    painter->fillRect( option.rect , brush );
   
    // draw border on selected items
    if ( option.state & QStyle::State_Selected )
    {
        static const int selectedBorderWidth = 6;


        painter->setBrush( QBrush(Qt::NoBrush) );
        QPen pen;
        pen.setBrush(option.palette.highlight());
        pen.setWidth(selectedBorderWidth);
        pen.setJoinStyle(Qt::MiterJoin);
        
        painter->setPen(pen);
        painter->drawRect( option.rect.adjusted(selectedBorderWidth/2,
                                                selectedBorderWidth/2,
                                                -selectedBorderWidth/2,
                                                -selectedBorderWidth/2) );
    }

   /* const ColorEntry* entries = scheme->colorTable();
    const qreal colorRectWidth = qMin(option.rect.width(),256) / TABLE_COLORS;
    const qreal colorRectHeight = colorRectWidth;
    qreal x = 0;
    qreal y = option.rect.bottom() - colorRectHeight;

    for ( int i = 0 ; i < TABLE_COLORS ; i++ )
    {
        QRectF colorRect;
        colorRect.setLeft(x);
        colorRect.setTop(y);
        colorRect.setSize( QSizeF(colorRectWidth,colorRectHeight) );
        painter->fillRect( colorRect , QColor(entries[i].color));

        x += colorRectWidth;
    }*/

    // draw color scheme name using scheme's foreground color
    QPen pen(scheme->foregroundColor());
    painter->setPen(pen);

    // use bold text for active color scheme
    QFont itemFont = painter->font();
    if ( index.data(Qt::CheckStateRole) == Qt::Checked )
        itemFont.setBold(true);
    else
        itemFont.setBold(false);

    painter->setFont(itemFont);

    painter->drawText( option.rect , Qt::AlignCenter , 
                        index.data(Qt::DisplayRole).value<QString>() );

}

QSize ColorSchemeViewDelegate::sizeHint( const QStyleOptionViewItem& option,
                       const QModelIndex& /*index*/) const
{
    const int width = 200;
    qreal colorWidth = (qreal)width / TABLE_COLORS;
    int margin = 5;
    qreal heightForWidth = ( colorWidth * 2 ) + option.fontMetrics.height() + margin;

    // temporary
    return QSize(width,(int)heightForWidth);
}

/*bool ColorSchemeViewDelegate::editorEvent(QEvent* event,QAbstractItemModel* model,
                             const QStyleOptionViewItem& option, const QModelIndex& index)
{
    qDebug() << "event: " << event->type() << " at row " << index.row() << " column " << 
        index.column();
    return false;
}*/

KeyBindingViewDelegate::KeyBindingViewDelegate(QObject* parent)
    : QAbstractItemDelegate(parent)
{
}
void KeyBindingViewDelegate::paint(QPainter* /*painter*/, 
                                   const QStyleOptionViewItem& /*option*/,
                                   const QModelIndex& /*index*/) const
{
}
QSize KeyBindingViewDelegate::sizeHint( const QStyleOptionViewItem& /*option*/,
                                        const QModelIndex& /*index*/) const
{
    // temporary
    return QSize(100,100);
}


#include "EditProfileDialog.moc"
