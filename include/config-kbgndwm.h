/*
 * config-kbgndwm.h.  Part of the KDE Project.
 *
 * Copyright (C) 1997 Martin Jones
 *               1998 Matej Koss
 *
 *
 */


#ifndef CONFIG_KBGNDWM_H
#define CONFIG_KBGNDWM_H

//one background for all desktops
#define DEFAULT_ENABLE_COMMON_BGND false

//default dock in the panel behaviour
#define DEFAULT_ENABLE_DOCKING true

//default color settings
#define DEFAULT_COLOR_1 "#4682B4"
#define DEFAULT_COLOR_2 "#4682B4"
#define DEFAULT_NUMBER_OF_COLORS 1
#define DEFAULT_COLOR_MODE Flat
#define DEFAULT_ORIENTATION_MODE Portrait

//default wallpaper settings
#define DEFAULT_USE_WALLPAPER false
#define DEFAULT_WALLPAPER_MODE Tiled
#define DEFAULT_WALLPAPER i18n("No wallpaper")

//default cache setting
#define DEFAULT_CACHE_SIZE 1024

//default random desktop background settings
#define DEFAULT_ENABLE_RANDOM_MODE false
#define DEFAULT_RANDOM_COUNT 1
#define DEFAULT_RANDOM_TIMER 600
#define DEFAULT_RANDOM_IN_ORDER false
#define DEFAULT_RANDOM_USE_DIR false
#define DEFAULT_DESKTOP 0

#endif //CONFIG_KBGNDWM_H

