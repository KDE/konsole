/*
    SPDX-FileCopyrightText: 2020-2020 Gustavo Carneiro <gcarneiroa@hotmail.com>
    SPDX-FileCopyrightText: 2012-2020 Kurt Hindenburg <kurt.hindenburg@gmail.com>
    SPDX-FileCopyrightText: 2020-2020 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

// Own
#include "KonsolePrintManager.h"

// Konsole
#include "PrintOptions.h"

// Qt
#include <QFont>
#include <QPainter>
#include <QPoint>
#include <QPointer>
#include <QPrintDialog>
#include <QPrinter>
#include <QRect>
#include <QWidget>

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
    QObject::connect(dialog, QOverload<>::of(&QPrintDialog::accepted), options, &Konsole::PrintOptions::saveSettings);
    if (dialog->exec() != QDialog::Accepted) {
        return;
    }

    QPainter painter;
    painter.begin(&printer);

    KConfigGroup configGroup(KSharedConfig::openConfig(), "PrintOptions");

    if (configGroup.readEntry("ScaleOutput", true)) {
        QRect page_rect = printer.pageLayout().paintRectPixels(printer.resolution());
        double scale = qMin(page_rect.width() / static_cast<double>(parent->width()), page_rect.height() / static_cast<double>(parent->height()));
        painter.scale(scale, scale);
    }

    pContent(painter, configGroup.readEntry("PrinterFriendly", true));
}

void KonsolePrintManager::printContent(QPainter &painter, bool friendly, QPoint columnsLines, pVTFontGet vtFontGet, pVTFontSet vtFontSet)
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
