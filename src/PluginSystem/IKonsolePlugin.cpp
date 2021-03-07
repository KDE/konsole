/*  This file was part of the KDE libraries

    SPDX-FileCopyrightText: 2019 Tomaz Canabrava <tcanabrava@kde.org>
    SPDX-FileCopyrightText: 2019 Martin Sandsmark <martin.sandsmark@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "IKonsolePlugin.h"

namespace Konsole {
struct IKonsolePlugin::Private {
    QString name;
};

IKonsolePlugin::IKonsolePlugin(QObject *parent, const QVariantList &args) :
    d(std::make_unique<IKonsolePlugin::Private>())
{

}

IKonsolePlugin::~IKonsolePlugin() = default;

TerminalDisplay * IKonsolePlugin::currentTerminalDisplay()
{
    return nullptr;
}

void IKonsolePlugin::setName(const QString& name)
{
    d->name = name;
}

QString IKonsolePlugin::name() const {
    return d->name;
}

}
