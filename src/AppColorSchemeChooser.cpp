/*************************************************************************************
 * SPDX-FileCopyrightText: 2016 Zhigalin Alexander <alexander@zhigalin.tk>
 * SPDX-FileCopyrightText: 2021 DI DIO Maximilien <maximilien.didio@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
 *************************************************************************************/

#include "AppColorSchemeChooser.h"
#include "konsoledebug.h"

#include <QMenu>
#include <QModelIndex>

#include <KActionCollection>
#include <KActionMenu>
#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

AppColorSchemeChooser::AppColorSchemeChooser(QObject *parent)
    : QAction(parent)
{
    auto *manager = new KColorSchemeManager(parent);

    const QString scheme(currentSchemeName());
    qCDebug(KonsoleDebug) << "Color scheme : " << scheme;

    auto *selectionMenu = manager->createSchemeSelectionMenu(scheme, this);

    connect(selectionMenu->menu(), &QMenu::triggered, this, &AppColorSchemeChooser::slotSchemeChanged);

    manager->activateScheme(manager->indexForScheme(scheme));

    setMenu(selectionMenu->menu());
    menu()->setIcon(QIcon::fromTheme(QStringLiteral("preferences-desktop-color")));
    menu()->setTitle(i18n("&Window Color Scheme"));
}

QString AppColorSchemeChooser::currentSchemeName() const
{
    if (!menu()) {
        KSharedConfigPtr config = KSharedConfig::openConfig();
        KConfigGroup cg(config, "UiSettings");
        return cg.readEntry("WindowColorScheme", QString());
    }

    if (QAction *const action = menu()->activeAction()) {
        return KLocalizedString::removeAcceleratorMarker(action->text());
    }

    return QString();
}

void AppColorSchemeChooser::slotSchemeChanged(QAction *triggeredAction)
{
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup cg(config, "UiSettings");
    cg.writeEntry("WindowColorScheme", KLocalizedString::removeAcceleratorMarker(triggeredAction->text()));
    cg.sync();
}
