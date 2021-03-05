/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
    SPDX-FileCopyrightText: 2019 Martin Sandsmark <martin.sandsmark@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PLUGINMANAGER_H
#define PLUGINMANAGER_H

#include <QObject>

#include <QList>

#include <memory>
#include <KService>
#include "konsole_export.h"

namespace Konsole {
class IKonsolePlugin;

class KONSOLE_EXPORT PluginManager : public QObject {
    Q_OBJECT
public:
    PluginManager();
    ~PluginManager();
    void loadAllPlugins();

    std::vector<IKonsolePlugin*> plugins() const;

private:
    struct Private;
    std::unique_ptr<Private> d;
};

}
#endif
