# Konsole - KDE's Terminal Emulator

Konsole is a terminal program for KDE.


## HOWTO Build

1. Install dependencies. On neon:
```
apt install git cmake make g++ extra-cmake-modules qtbase5-dev libkf5config-dev libkf5auth-dev libkf5package-dev libkf5declarative-dev libkf5coreaddons-dev libkf5kcmutils-dev libkf5i18n-dev libqt5core5a libqt5widgets5 libqt5gui5 libqt5qml5 extra-cmake-modules qtbase5-dev kdelibs5-dev qt5-default libkf5notifyconfig-dev libkf5pty-dev libkf5notifications-dev libkf5parts-dev
```
2. Clone with `git clone https://invent.kde.org/utilities/konsole.git`
3. Make _build_ directory: `mkdir konsole/build`
4. Change into _build_ directory: `cd konsole/build`
5. Configure: `cmake ..` (or `cmake .. -DCMAKE_INSTALL_PREFIX=/where/your/want/to/install`)
6. Build: `make`
7. Install: `make install`



