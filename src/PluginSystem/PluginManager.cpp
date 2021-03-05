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

PluginManager::PluginManager() {
}

PluginManager::~PluginManager() noexcept = default;



void PluginManager::loadAllPlugins()
{
    KService::List services = KServiceTypeTrader::self()->query(QStringLiteral("Konsole/Plugin"));

    for(KService::Ptr service : services)
    {
        KPluginFactory *factory = KPluginLoader(service->library()).factory();
        if (!factory)
        {
            qDebug() << "KPluginFactory could not load the plugin:" << service->library();
            continue;
        }

        auto *plugin = factory->create<IKonsolePlugin>(this);
        d->plugins.push_back(plugin);
    }
}

std::vector<IKonsolePlugin*> PluginManager::plugins() const
{
    return d->plugins;
}

}
