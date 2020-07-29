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
#include "ColorSchemeViewDelegate.h"

// Konsole
#include "ColorScheme.h"

// KDE
#include <KWindowSystem>
#include <KLocalizedString>

// Qt
#include <QPainter>
#include <QApplication>

using namespace Konsole;

ColorSchemeViewDelegate::ColorSchemeViewDelegate(QObject *parent)
    : QAbstractItemDelegate(parent)
{
}

void ColorSchemeViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    const auto *scheme = index.data(Qt::UserRole + 1).value<const ColorScheme *>();
    QFont profileFont = index.data(Qt::UserRole + 2).value<QFont>();
    Q_ASSERT(scheme);
    if (scheme == nullptr) {
        return;
    }

    painter->setRenderHint(QPainter::Antialiasing);

    // Draw background
    QStyle *style = option.widget != nullptr ? option.widget->style() : QApplication::style();
    style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, option.widget);

    // Draw name
    QPalette::ColorRole textColor = ((option.state & QStyle::State_Selected) != 0)
                                    ? QPalette::HighlightedText : QPalette::Text;
    painter->setPen(option.palette.color(textColor));
    painter->setFont(option.font);

    // Determine width of sample text using profile's font
    const QString sampleText = i18n("AaZz09...");
    QFontMetrics profileFontMetrics(profileFont);
    const int sampleTextWidth = profileFontMetrics.boundingRect(sampleText).width();

    painter->drawText(option.rect.adjusted(sampleTextWidth + 15, 0, 0, 0),
                      Qt::AlignLeft | Qt::AlignVCenter,
                      index.data(Qt::DisplayRole).toString());

    // Draw the preview
    const int x = option.rect.left();
    const int y = option.rect.top();

    QRect previewRect(x + 4, y + 4, sampleTextWidth + 8, option.rect.height() - 8);

    bool transparencyAvailable = KWindowSystem::compositingActive();

    if (transparencyAvailable) {
        painter->save();
        QColor color = scheme->backgroundColor();
        color.setAlphaF(scheme->opacity());
        painter->setPen(Qt::NoPen);
        painter->setCompositionMode(QPainter::CompositionMode_Source);
        painter->setBrush(color);
        painter->drawRect(previewRect);
        painter->restore();
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(scheme->backgroundColor());
        painter->drawRect(previewRect);
    }

    // draw color scheme name using scheme's foreground color
    QPen pen(scheme->foregroundColor());
    painter->setPen(pen);

    // TODO: respect antialias setting
    painter->setFont(profileFont);
    painter->drawText(previewRect, Qt::AlignCenter, sampleText);
}

QSize ColorSchemeViewDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex & /*index*/) const
{
    const int width = 200;
    const int margin = 5;
    const int colorWidth = width / TABLE_COLORS;
    const int heightForWidth = (colorWidth * 2) + option.fontMetrics.height() + margin;

    // temporary
    return {width, heightForWidth};
}
