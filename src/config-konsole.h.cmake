#define KONSOLE_VERSION "${RELEASE_SERVICE_VERSION}"

/* Defined if on DragonFly BSD */
#cmakedefine01 HAVE_OS_DRAGONFLYBSD

#cmakedefine01 WITH_X11

#cmakedefine01 HAVE_DBUS

/* If defined, can checksum rectangular areas of the screen */
#cmakedefine01 ENABLE_DECRQCRA

#cmakedefine01 HAVE_GETPWUID

/* Defined if system has the malloc_trim function, which is a GNU extension */
#cmakedefine01 HAVE_MALLOC_TRIM
