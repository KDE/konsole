/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
    SPDX-FileCopyrightText: 2019 Martin Sandsmark <martin.sandsmark@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IKONSOLEPLUGIN_H
#define IKONSOLEPLUGIN_H

#include <QList>
#include <QDockWidget>

#include <terminalDisplay/TerminalDisplay.h>

#include <KXMLGUIClient>
#include <QObject>

#include <KPluginFactory>
#include <KExportPlugin>

#include <memory>

#include "konsole_export.h"

namespace Konsole {

class KONSOLE_EXPORT IKonsolePlugin : public QObject {
    Q_OBJECT
public:
    IKonsolePlugin(QObject *parent, const QVariantList &args);
    ~IKonsolePlugin();

    /* Returns the list of widgets that should be used on the dock areas. */
    virtual QList<QDockWidget*> widgets() { return {}; }

    /* Always returns the current terminal display.
     * never store the pointer, as it can change during usage.
     */
    TerminalDisplay *currentTerminalDisplay();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

}
#endif
