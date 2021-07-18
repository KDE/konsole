# Konsole - KDE's Terminal Emulator

Konsole is a terminal program for KDE.

As well as being a standalone program, it is also used by other KDE programs
such as the Kate editor and KDevelop development environment to provide easy
access to a terminal window. Konsole's features and usage are explained and
illustrated in the Konsole handbook, which can be accessed by browsing to
`help:/konsole` in Konqueror.


## Directory Structure

| Directory          | Description                                                                                                                                                                        |
| ------------------ | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `/doc/user`        | README files, primarily for advanced users, explaining various aspects of Konsole such as fonts and keyboard handling in-depth.                                                    |
| `/doc/developer`   | README files and resources for developers of Konsole. This includes information on the design of Konsole's internals and the VT100 terminal on which Konsole's emulation is based. |
| `/src`             | Source code for Konsole, including the embedded versions of Konsole which are used in Kate, KDevelop and others.                                                                   |
| `/desktop`         | .desktop files for Konsole, used to launch the program from KDE's various menus and other application launchers.                                                                   |
| `/data`            | Data files for use with Konsole as well as the keyboard setup and color schemes provided with Konsole.                                                                             |


## HOWTO Build

1. Install dependencies. On neon:
```
apt install git cmake make g++ extra-cmake-modules qtbase5-dev libkf5config-dev libkf5auth-dev libkf5package-dev libkf5declarative-dev libkf5coreaddons-dev libkf5kcmutils-dev libkf5i18n-dev libqt5core5a libqt5widgets5 libqt5gui5 libqt5qml5 extra-cmake-modules qtbase5-dev kdelibs5-dev qt5-default libkf5notifyconfig-dev libkf5pty-dev libkf5notifications-dev libkf5parts-dev
```
2. Clone with `git clone https://invent.kde.org/utilities/konsole.git`
3. Make _build_ directory: `mkdir konsole\build`
4. Change into _build_ directory: `cd konsole\build`
5. Configure: `cmake ..` (or `cmake .. -DCMAKE_INSTALL_PREFIX=/where/your/want/to/install`)
6. Build: `make`
7. Install: `make install`

## Contact

Up-to-date information about the latest releases can be found on Konsole's
website at https://konsole.kde.org. Discussions about Konsole's development are
held on the konsole-devel mailing list, which can be accessed at
https://mail.kde.org/mailman/listinfo/konsole-devel.

## Quick Links
- [KDE Release Schedule](https://community.kde.org/Schedules)
- [Official Homepage](https://konsole.kde.org)
- [Builds](https://build.kde.org/job/Applications/job/konsole)
- [Forums](http://forum.kde.org/viewforum.php?f=227)
- [Konsole Bug Reports per Component](https://bugs.kde.org/describecomponents.cgi?product=konsole)

