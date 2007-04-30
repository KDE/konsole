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
#include <KDirSelectDialog>

// Konsole
#include "ColorScheme.h"
#include "ui_EditProfileDialog.h"
#include "EditProfileDialog.h"
#include "SessionManager.h"

using namespace Konsole;

EditProfileDialog::EditProfileDialog(QWidget* parent)
    : KDialog(parent)
{
    setCaption("Edit Profile");
    setButtons( KDialog::Ok | KDialog::Cancel | KDialog::Apply | KDialog::Default );

    _ui = new Ui::EditProfileDialog();
    _ui->setupUi(mainWidget());
}
EditProfileDialog::~EditProfileDialog()
{
    delete _ui;
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
}
void EditProfileDialog::setupGeneralPage(const Profile* info)
{
    _ui->sessionNameEdit->setText( info->name() );
    _ui->commandEdit->setText( info->command(false,false) );
    _ui->initialDirEdit->setText( info->defaultWorkingDirectory() );
    _ui->iconSelectButton->setIcon( KIcon(info->icon()) );

    // signals and slots
    connect( _ui->dirSelectButton , SIGNAL(clicked()) , this , SLOT(selectInitialDir()) );
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
    const QFont& font = info->defaultFont();
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
void EditProfileDialog::setupScrollingPage(const Profile* )
{
}
void EditProfileDialog::setupAdvancedPage(const Profile* )
{
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
    } 
}
void EditProfileDialog::setFontSize(int pointSize)
{
    QFont newFont = _ui->fontPreviewLabel->font();
    newFont.setPointSize(pointSize);
    _ui->fontPreviewLabel->setFont( newFont );
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
    const qreal colorRectWidth = option.rect.width() / TABLE_COLORS;
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
