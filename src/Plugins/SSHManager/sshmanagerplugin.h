#ifndef SSHMANAGERPLUGIN_H
#define SSHMANAGERPLUGIN_H

#include <IKonsolePlugin.h>

#include <memory>

namespace Konsole {
    class SessionController;
}

class SSHManagerPlugin : public Konsole::IKonsolePlugin {
    Q_OBJECT
public:
    SSHManagerPlugin(QObject *object, const QVariantList &args);
    ~SSHManagerPlugin();

    void createWidgetsForMainWindow(QMainWindow *mainWindow) override;
    void activeViewChanged(Konsole::SessionController *controller) override;

private:
    struct Private;
    std::unique_ptr<SSHManagerPlugin::Private> d;
};

#endif
