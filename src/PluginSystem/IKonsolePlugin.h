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

    QString name() const;

    // Usable only from PluginManager, please don't use.
    void addMainWindow(QMainWindow *mainWindow);
    void removeMainWindow(QMainWindow *mainWindow);

    virtual void createWidgetsForMainWindow(QMainWindow *mainWindow) = 0;
    virtual void activeViewChanged(Konsole::SessionController *controller) = 0;

protected:
    void setName(const QString& pluginName);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

}
#endif
