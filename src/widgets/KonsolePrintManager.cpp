/*
    Copyright 2020-2020 by Gustavo Carneiro <gcarneiroa@hotmail.com>
    Copyright 2012-2020 by Kurt Hindenburg <kurt.hindenburg@gmail.com>
    Copyright 2020-2020 by Tomaz Canabrava <tcanabrava@kde.org>

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
#include "KonsolePrintManager.h"

// Konsole
#include "PrintOptions.h"

// Qt
#include <QRect>
#include <QFont>
#include <QPoint>
#include <QWidget>
#include <QPrinter>
#include <QPainter>
#include <QPointer>
#include <QPrintDialog>

// KDE
#include <KConfigGroup>
#include <KSharedConfig>

using namespace Konsole;

KonsolePrintManager::KonsolePrintManager(pDrawBackground drawBackground, pDrawContents drawContents, pColorGet colorGet)
{
    _drawBackground = drawBackground;
    _drawContents = drawContents;
    _backgroundColor = colorGet;
}

void KonsolePrintManager::printRequest(pPrintContent pContent, QWidget *parent)
{
    if (!pContent) {
        return;
    }

    QPrinter printer;

    QPointer<QPrintDialog> dialog = new QPrintDialog(&printer, parent);
    auto options = new PrintOptions();

    dialog->setOptionTabs({options});
    dialog->setWindowTitle(i18n("Print Shell"));
    QObject::connect(dialog,
            QOverload<>::of(&QPrintDialog::accepted),
            options,
            &Konsole::PrintOptions::saveSettings);
    if (dialog->exec() != QDialog::Accepted) {
        return;
    }

    QPainter painter;
    painter.begin(&printer);

    KConfigGroup configGroup(KSharedConfig::openConfig(), "PrintOptions");

    if (configGroup.readEntry("ScaleOutput", true)) {
        double scale = qMin(printer.pageRect().width() / static_cast<double>(parent->width()),
                            printer.pageRect().height() / static_cast<double>(parent->height()));
        painter.scale(scale, scale);
    }
    
    pContent(painter, configGroup.readEntry("PrinterFriendly", true));
}

void KonsolePrintManager::printContent(QPainter &painter, bool friendly, QPoint columnsLines,
                                        pVTFontGet vtFontGet, pVTFontSet vtFontSet)
{
    // Reinitialize the font with the printers paint device so the font
    // measurement calculations will be done correctly
    QFont savedFont = vtFontGet();
    QFont font(savedFont, painter.device());
    painter.setFont(font);
    vtFontSet(font);

    QRect rect(0, 0, columnsLines.y(), columnsLines.x());

    if (!friendly) {
        _drawBackground(painter, rect, _backgroundColor(), true);
    }
    _drawContents(painter, rect, friendly);
    vtFontSet(savedFont);
}
