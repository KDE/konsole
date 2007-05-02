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
#include "ui_EditProfileDialog.h"
#include "EditProfileDialog.h"
#include "EditTabTitleFormatDialog.h"
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
}
void EditProfileDialog::accept()
{
    save();
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
    _ui->commandEdit->setText( info->command() );

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
                            {_ui->alwaysShowTabBarButton,Profile::AlwaysShowTabBar,SLOT(showTabBarAsNeeded())},
                            {_ui->autoShowTabBarButton,Profile::ShowTabBarAsNeeded,SLOT(alwaysShowTabBar())},
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
    QStandardItemModel* model = new QStandardItemModel(this);
    QList<ColorScheme*> schemeList = ColorSchemeManager::instance()->allColorSchemes();
    QListIterator<ColorScheme*> schemeIter(schemeList);

    while (schemeIter.hasNext())
    {
        ColorScheme* colors = schemeIter.next();
        QStandardItem* item = new QStandardItem(colors->name());
        item->setData( QVariant::fromValue(colors) ,  Qt::UserRole + 1);

        model->appendRow(item);
    }

    _ui->colorSchemeList->setModel(model);
    _ui->colorSchemeList->setItemDelegate(new ColorSchemeViewDelegate(this));
    
    // setup font preview
    const QFont& font = info->font();
    _ui->fontPreviewLabel->setFont( font );
    _ui->fontSizeSlider->setValue( font.pointSize() );

    connect( _ui->fontSizeSlider , SIGNAL(valueChanged(int)) , this ,
             SLOT(setFontSize(int)) );
    connect( _ui->editFontButton , SIGNAL(clicked()) , this ,
             SLOT(showFontDialog()) );
}
void EditProfileDialog::setupKeyboardPage(const Profile* )
{
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
void EditProfileDialog::showFontDialog()
{
    //TODO Only permit selection of mono-spaced fonts.  
    // the KFontDialog API does not appear to have a means to do this
    // at present.
    QFont currentFont = _ui->fontPreviewLabel->font();
    
    if ( KFontDialog::getFont(currentFont) == KFontDialog::Accepted )
    {
        QSlider* slider = _ui->fontSizeSlider;

        _ui->fontSizeSlider->setRange( qMin(slider->minimum(),currentFont.pointSize()) ,
                                       qMax(slider->maximum(),currentFont.pointSize()) );
        _ui->fontSizeSlider->setValue(currentFont.pointSize());
        _ui->fontPreviewLabel->setFont(currentFont);

        _tempProfile->setProperty(Profile::Font,currentFont);
    } 
}
void EditProfileDialog::setFontSize(int pointSize)
{
    QFont newFont = _ui->fontPreviewLabel->font();
    newFont.setPointSize(pointSize);
    _ui->fontPreviewLabel->setFont( newFont );

    _tempProfile->setProperty(Profile::Font,newFont);
}
ColorSchemeViewDelegate::ColorSchemeViewDelegate(QObject* parent)
 : QAbstractItemDelegate(parent)
{

}
void ColorSchemeViewDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                       const QModelIndex& index) const
{
    ColorScheme* scheme = index.data(Qt::UserRole + 1).value<ColorScheme*>();

    Q_ASSERT(scheme);

    QLinearGradient gradient(option.rect.topLeft(),option.rect.bottomRight());
    gradient.setColorAt(0,scheme->foregroundColor());
    gradient.setColorAt(1,scheme->backgroundColor());
    QBrush brush(scheme->backgroundColor());

    painter->fillRect( option.rect , brush );
    
    const ColorEntry* entries = scheme->colorTable();
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
    }

    QPen pen(scheme->foregroundColor());
    painter->setPen(pen);

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
