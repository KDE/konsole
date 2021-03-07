/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
    SPDX-FileCopyrightText: 2019 Martin Sandsmark <martin.sandsmark@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "PluginManager.h"

#include <KServiceTypeTrader>

#include "IKonsolePlugin.h"

namespace Konsole {

struct PluginManager::Private {
    std::vector<IKonsolePlugin*> plugins;
};

PluginManager::PluginManager()
: d(std::make_unique<PluginManager::Private>())
{
}

PluginManager::~PluginManager() noexcept = default;



void PluginManager::loadAllPlugins()
{
    QList<QObject*> pluginFactories = KPluginLoader::instantiatePlugins(QStringLiteral("konsoleplugins"));
    for(QObject *pluginFactory : pluginFactories) {
        auto factory = qobject_cast<KPluginFactory*>(pluginFactory);
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

std::vector<IKonsolePlugin*> PluginManager::plugins() const
{
    return d->plugins;
}

}
