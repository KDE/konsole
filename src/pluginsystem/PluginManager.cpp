/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2021 Tomaz Canabrava <tcanabrava@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PluginManager.h"

#include "IKonsolePlugin.h"
#include "MainWindow.h"

#include "kcoreaddons_version.h"
#include <KLocalizedString>
#include <KPluginFactory>
#include <KPluginMetaData>

#if KCOREADDONS_VERSION < QT_VERSION_CHECK(5, 86, 0)
#include <KPluginLoader>
#endif

#include <QAction>

namespace Konsole
{
struct PluginManagerPrivate {
    std::vector<IKonsolePlugin *> plugins;
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
#if KCOREADDONS_VERSION < QT_VERSION_CHECK(5, 86, 0)
    QVector<KPluginMetaData> pluginMetaData = KPluginLoader::findPlugins(QStringLiteral("konsoleplugins"));
    for (const auto &metaData : pluginMetaData) {
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
#else
    QVector<KPluginMetaData> pluginMetaData = KPluginMetaData::findPlugins(QStringLiteral("konsoleplugins"));
    for (const auto &metaData : pluginMetaData) {
        const KPluginFactory::Result result = KPluginFactory::instantiatePlugin<IKonsolePlugin>(metaData);
        if (!result) {
            continue;
        }

        d->plugins.push_back(result.plugin);
    }
#endif
}

void PluginManager::registerMainWindow(Konsole::MainWindow *window)
{
    window->unplugActionList(QStringLiteral("plugin-submenu"));

    QList<QAction *> internalPluginSubmenus;
    for (auto *plugin : d->plugins) {
        plugin->addMainWindow(window);
        internalPluginSubmenus.append(plugin->menuBarActions(window));
        window->addPlugin(plugin);
    }

    if (internalPluginSubmenus.isEmpty()) {
        auto *emptyMenuAct = new QAction(i18n("No plugins available"), this);
        emptyMenuAct->setEnabled(false);
        internalPluginSubmenus.append(emptyMenuAct);
    }

    window->plugActionList(QStringLiteral("plugin-submenu"), internalPluginSubmenus);
}

std::vector<IKonsolePlugin *> PluginManager::plugins() const
{
    return d->plugins;
}

}
