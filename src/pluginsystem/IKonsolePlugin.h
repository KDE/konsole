/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IKONSOLEPLUGIN_H
#define IKONSOLEPLUGIN_H

#include <QList>
#include <QObject>

#include <terminalDisplay/TerminalDisplay.h>

#include <KPluginFactory>

#include <memory>

#include "konsoleapp_export.h"

namespace Konsole
{
class MainWindow;

class KONSOLEAPP_EXPORT IKonsolePlugin : public QObject
{
    Q_OBJECT
public:
    IKonsolePlugin(QObject *parent, const QVariantList &args);
    ~IKonsolePlugin() override;

    QString name() const;

    // Usable only from PluginManager, please don't use.
    void addMainWindow(Konsole::MainWindow *mainWindow);
    void removeMainWindow(Konsole::MainWindow *mainWindow);

    virtual void createWidgetsForMainWindow(Konsole::MainWindow *mainWindow) = 0;
    virtual void activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *mainWindow) = 0;

    virtual QList<QAction *> menuBarActions(Konsole::MainWindow *mainWindow) const
    {
        Q_UNUSED(mainWindow)
        return {};
    }

protected:
    void setName(const QString &pluginName);

private:
    struct Private;
    std::unique_ptr<Private> d;
};

}
#endif
