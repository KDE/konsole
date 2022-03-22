#define KONSOLE_VERSION "${RELEASE_SERVICE_VERSION}"

/* Defined if on DragonFly BSD */
#cmakedefine HAVE_OS_DRAGONFLYBSD 1

#cmakedefine01 HAVE_X11

/* If defined, remove public access to dbus sendInput/runCommand */
#cmakedefine REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS

/* If defined, can checksum rectangular areas of the screen */
#cmakedefine ENABLE_DECRQCRA

#cmakedefine USE_TERMINALINTERFACEV2

#cmakedefine HAVE_GETPWUID ${HAVE_GETPWUID}

/* Defined if system has the malloc_trim function, which is a GNU extension */
#cmakedefine HAVE_MALLOC_TRIM
