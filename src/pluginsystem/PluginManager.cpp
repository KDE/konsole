/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PluginManager.h"

#include "IKonsolePlugin.h"
#include "MainWindow.h"

#include <KPluginLoader>
#include <KPluginMetaData>

namespace Konsole {

struct PluginManagerPrivate {
    std::vector<IKonsolePlugin*> plugins;
};

PluginManager::PluginManager()
: d(std::make_unique<PluginManagerPrivate>())
{
}

PluginManager::~PluginManager()
{
    qDeleteAll(d->plugins);
}

void PluginManager::loadAllPlugins()
{
    QVector<KPluginMetaData> pluginMetaData = KPluginLoader::findPlugins(QStringLiteral("konsoleplugins"));
    for(const auto &metaData : pluginMetaData) {
        KPluginLoader pluginLoader(metaData.fileName());
        KPluginFactory *factory = pluginLoader.factory();
        if (!factory) {
            continue;
        }

        auto *plugin = factory->create<IKonsolePlugin>();
        if (!plugin) {
            continue;
        }

        d->plugins.push_back(plugin);
    }
}

void PluginManager::registerMainWindow(Konsole::MainWindow* window)
{
    window->unplugActionList(QStringLiteral("plugin-submenu"));

    QList<QAction*> internalPluginSubmenus;
    for (auto *plugin : d->plugins) {
        plugin->addMainWindow(window);
        internalPluginSubmenus.append(plugin->menuBarActions(window));
        window->addPlugin(plugin);
    }

    window->plugActionList(QStringLiteral("plugin-submenu"), internalPluginSubmenus);
}

std::vector<IKonsolePlugin*> PluginManager::plugins() const
{
    return d->plugins;
}

}
