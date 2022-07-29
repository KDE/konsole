/*
    SPDX-FileCopyrightText: 2022 Vlad Zahorodnii <vlad.zahorodnii@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include <QAction>
#include <QCoreApplication>
#include <QKeySequence>

#include <KConfig>
#include <KConfigGroup>
#include <KGlobalAccel>
#include <KService>

#include <optional>

static void migrateShortcut(const QString &desktopFile, const QList<QKeySequence> &shortcuts)
{
    const KService::Ptr service = KService::serviceByStorageId(desktopFile);
    QAction action(service->name());
    action.setProperty("componentName", desktopFile);
    action.setProperty("componentDisplayName", service->name());
    action.setObjectName(QStringLiteral("_launch"));
    // Tell kglobalaccel that the action is active.
    KGlobalAccel::self()->setShortcut(&action, shortcuts);
    action.setProperty("isConfigurationAction", true);
    KGlobalAccel::self()->setShortcut(&action, shortcuts, KGlobalAccel::NoAutoloading);
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    KConfig khotkeysrc(QStringLiteral("khotkeysrc"), KConfig::SimpleConfig);
    const int dataCount = KConfigGroup(&khotkeysrc, "Data").readEntry("DataCount", 0);
    std::optional<int> kmenueditIndex = std::nullopt;
    KConfigGroup kmenueditGroup;
    for (int i = 1; i <= dataCount; ++i) {
        kmenueditGroup = KConfigGroup(&khotkeysrc, QStringLiteral("Data_%1").arg(i));
        if (kmenueditGroup.readEntry("Name") == QLatin1String("KMenuEdit")) {
            kmenueditIndex = i;
            break;
        }
    }
    if (!kmenueditIndex.has_value()) {
        return 0;
    }

    const int shortcutCount = kmenueditGroup.readEntry("DataCount", 0);
    for (int i = 1; i <= shortcutCount; ++i) {
        const QString groupName = QStringLiteral("Data_%1_%2").arg(kmenueditIndex.value()).arg(i);
        if (KConfigGroup(&khotkeysrc, groupName).readEntry("Type") == QLatin1String("MENUENTRY_SHORTCUT_ACTION_DATA")) {
            const QString desktopFile = KConfigGroup(&khotkeysrc, groupName + QStringLiteral("Actions0")).readEntry("CommandURL");
            if (desktopFile == QLatin1String("org.kde.konsole.desktop")) {
                const QString shortcutId = KConfigGroup(&khotkeysrc, groupName + QStringLiteral("Triggers0")).readEntry("Uuid");
                const QList<QKeySequence> shortcuts = KGlobalAccel::self()->globalShortcut(QStringLiteral("khotkeys"), shortcutId);

                // Unset old shortcut. setShortcut() is needed to make the action active.
                QAction action;
                action.setObjectName(shortcutId);
                action.setProperty("componentName", QStringLiteral("khotkeys"));
                KGlobalAccel::self()->setShortcut(&action, {});
                KGlobalAccel::self()->removeAllShortcuts(&action);

                // Migrate to kglobalaccel.
                migrateShortcut(desktopFile, shortcuts);

                // khotkeys will automagically update the DataCount key.
                khotkeysrc.deleteGroup(groupName);
                khotkeysrc.deleteGroup(groupName + QStringLiteral("Actions"));
                khotkeysrc.deleteGroup(groupName + QStringLiteral("Actions0"));
                khotkeysrc.deleteGroup(groupName + QStringLiteral("Conditions"));
                khotkeysrc.deleteGroup(groupName + QStringLiteral("Triggers"));
                khotkeysrc.deleteGroup(groupName + QStringLiteral("Triggers0"));
            }
        }
    }

    khotkeysrc.sync();
    return 0;
}
