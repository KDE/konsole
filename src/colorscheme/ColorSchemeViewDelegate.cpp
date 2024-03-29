/*
    SPDX-FileCopyrightText: 2007-2008 Robert Knight <robertknight@gmail.com>
    SPDX-FileCopyrightText: 2018 Harald Sitter <sitter@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "ColorSchemeViewDelegate.h"

// Konsole
#include "../config-konsole.h"
#include "ColorScheme.h"

// KDE
#include <KLocalizedString>

// Qt
#include <QApplication>
#include <QPainter>

using namespace Konsole;

ColorSchemeViewDelegate::ColorSchemeViewDelegate(std::function<bool()> compositingActiveHelper, QObject *parent)
    : QAbstractItemDelegate(parent)
    , m_compositingActive(std::move(compositingActiveHelper))
{
    Q_ASSERT(m_compositingActive);
}

void ColorSchemeViewDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    std::shared_ptr<const ColorScheme> scheme = index.data(Qt::UserRole + 1).value<std::shared_ptr<const ColorScheme>>();
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
    QPalette::ColorRole textColor = ((option.state & QStyle::State_Selected) != 0) ? QPalette::HighlightedText : QPalette::Text;
    painter->setPen(option.palette.color(textColor));
    painter->setFont(option.font);

    // Determine width of sample text using profile's font
    const QString sampleText = i18n("AaZz09...");
    QFontMetrics profileFontMetrics(profileFont);
    const int sampleTextWidth = profileFontMetrics.boundingRect(sampleText).width();

    painter->drawText(option.rect.adjusted(sampleTextWidth + 15, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, index.data(Qt::DisplayRole).toString());

    // Draw the preview
    const int x = option.rect.left();
    const int y = option.rect.top();

    QRect previewRect(x + 4, y + 4, sampleTextWidth + 8, option.rect.height() - 8);

    if (m_compositingActive()) {
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

#include "moc_ColorSchemeViewDelegate.cpp"
