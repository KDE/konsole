#define KONSOLE_VERSION "${RELEASE_SERVICE_VERSION}"

/* Defined if on DragonFly BSD */
#cmakedefine01 HAVE_OS_DRAGONFLYBSD

#cmakedefine01 HAVE_X11

/* If defined, remove public access to dbus sendInput/runCommand */
#cmakedefine01 REMOVE_SENDTEXT_RUNCOMMAND_DBUS_METHODS

/* If defined, can checksum rectangular areas of the screen */
#cmakedefine01 ENABLE_DECRQCRA

#cmakedefine01 USE_TERMINALINTERFACEV2

#cmakedefine01 HAVE_GETPWUID

/* Defined if system has the malloc_trim function, which is a GNU extension */
#cmakedefine01 HAVE_MALLOC_TRIM
