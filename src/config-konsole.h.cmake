#define KONSOLE_VERSION "${KDE_APPLICATIONS_VERSION}"

/* Defined if you want detaching enabled */
#cmakedefine ENABLE_DETACHING 1

/* Defined if on DragonFly BSD */
#cmakedefine HAVE_OS_DRAGONFLYBSD 1

#cmakedefine01 HAVE_X11

/* If defined, remove public access to dbus sendInput/runCommand */
#cmakedefine REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS
