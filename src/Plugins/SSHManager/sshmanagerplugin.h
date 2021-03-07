#ifndef SSHMANAGERPLUGIN_H
#define SSHMANAGERPLUGIN_H

#include <IKonsolePlugin.h>

class SSHManagerPlugin : public Konsole::IKonsolePlugin {
    Q_OBJECT
public:
    SSHManagerPlugin(QObject *object, const QVariantList &args);
    void createWidgetsForMainWindow(QMainWindow *mainWindow) override;
};

#endif
