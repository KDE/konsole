#include "Terminal1IntentHandler.h"

#if HAVE_DBUS
#include <QDBusMetaType>
#include <QProcessEnvironment>
#include <QVariant>
#include <ranges>

using namespace Qt::StringLiterals;

void Terminal1IntentHandler::LaunchCommand(const QList<QVariantMap> &commands,
                                           const QByteArray &desktopEntry,
                                           const QVariantMap &options,
                                           const QVariantMap &platformData)
{
    qDBusRegisterMetaType<QList<QVariantMap>>();
    qDBusRegisterMetaType<QList<QByteArray>>();

    qDebug() << commands << desktopEntry << options << platformData;

    struct Command {
        QList<QString> exec;
        QProcessEnvironment envTweaks;
        QString workingDir;
    };

    auto parseCommand = [](const QVariantMap &command) {
        const auto exec = command.value(u"exec"_s).value<QList<QByteArray>>() | std::views::transform([](const auto &bytes) {
                              return QString::fromUtf8(bytes);
                          });
        const auto workingDir = QString::fromUtf8(command.value(u"workind_directory"_s).toByteArray());
        const auto rawEnv = command.value(u"env"_s).value<QList<QByteArray>>();
        QProcessEnvironment tweaks;
        for (const QByteArrayView raw : rawEnv) {
            const auto delim = raw.indexOf(u'=');
            if (delim != -1) {
                tweaks.insert(QString::fromUtf8(raw.first(delim)), QString::fromUtf8(raw.sliced(delim + 1)));
            }
        }
        return Command{.exec = {exec.begin(), exec.end()}, .envTweaks = tweaks, .workingDir = workingDir};
    };

    auto parsed = commands | std::views::transform(parseCommand);
}

#include "moc_Terminal1IntentHandler.cpp"
#endif
