/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>

#include <QList>

#include "konsole_export.h"
#include <memory>

namespace Konsole
{
class IKonsolePlugin;
class MainWindow;

struct PluginManagerPrivate;

class KONSOLE_EXPORT PluginManager : public QObject
{
    Q_OBJECT
public:
    PluginManager();
    ~PluginManager() override;
    void loadAllPlugins();
    void registerMainWindow(Konsole::MainWindow *window);

    std::vector<IKonsolePlugin *> plugins() const;

private:
    std::unique_ptr<PluginManagerPrivate> d;
};

}
#endif
