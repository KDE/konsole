/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IKONSOLEPLUGIN_H
#define IKONSOLEPLUGIN_H

#include <QList>
#include <QDockWidget>
#include <QObject>

#include <terminalDisplay/TerminalDisplay.h>

#include <KXMLGUIClient>
#include <KPluginFactory>
#include <KExportPlugin>

#include <memory>

#include "konsole_export.h"

namespace Konsole {
class MainWindow;

class KONSOLE_EXPORT IKonsolePlugin : public QObject {
    Q_OBJECT
public:
    IKonsolePlugin(QObject *parent, const QVariantList &args);
    ~IKonsolePlugin();

    QString name() const;

    // Usable only from PluginManager, please don't use.
    void addMainWindow(Konsole::MainWindow *mainWindow);
    void removeMainWindow(Konsole::MainWindow *mainWindow);

    virtual void createWidgetsForMainWindow(Konsole::MainWindow *mainWindow) = 0;
    virtual void activeViewChanged(Konsole::SessionController *controller) = 0;

    virtual QList<QAction*> menuBarActions(Konsole::MainWindow* mainWindow) const { return {}; };

protected:
    void setName(const QString& pluginName);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

}
#endif
