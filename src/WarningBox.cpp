/*
    Copyright 2008 by Robert Knight <robertknight@gmail.com>

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
#include "WarningBox.h"

// Qt
#include <QLabel>
#include <QHBoxLayout>

// KDE
#include <KIcon>
#include <KColorScheme>
#include <KDebug>

using namespace Konsole;

WarningBox::WarningBox(QWidget* parent)
: QFrame(parent)
{
    KColorScheme colorScheme(QPalette::Active);
    QColor warningColor = colorScheme.background(KColorScheme::NeutralBackground).color();
    QColor warningColorLight = KColorScheme::shade(warningColor,KColorScheme::LightShade,0.1); 
    QColor borderColor = KColorScheme::shade(warningColor,KColorScheme::DarkShade,0.15);
    QString gradient =     "qlineargradient(x1:0, y1:0, x2:0, y2:1,"
                        "stop: 0 %1, stop: 0.6 %1 ,stop: 1.0 %2)";
    gradient = gradient.arg(warningColor.name()).arg(warningColorLight.name());

    QString styleSheet = "Konsole--WarningBox { background: %1;"
                         "border: 2px solid %2; }";
    setStyleSheet(styleSheet.arg(gradient).arg(borderColor.name()));

    _label = new QLabel();
    _label->setWordWrap(true);
    _label->setAlignment(Qt::AlignLeft);

    QLabel* icon = new QLabel();
    icon->setPixmap(KIcon("dialog-warning").pixmap(QSize(48,48)));
    icon->setAlignment(Qt::AlignCenter);

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->addWidget(icon);
    layout->addWidget(_label);
    layout->setStretchFactor(icon,2);
    layout->setStretchFactor(_label,5);
}
void WarningBox::setText(const QString& text)
{
    _label->setText(text);
}
QString WarningBox::text() const
{
    return _label->text();
}

#include "WarningBox.moc"


