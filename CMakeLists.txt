# Konsole project

# KDE Application Version, managed by release script
set (RELEASE_SERVICE_VERSION_MAJOR "23")
set (RELEASE_SERVICE_VERSION_MINOR "08")
set (RELEASE_SERVICE_VERSION_MICRO "5")
set (RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

# Do not increase these requirements without a merge-request or/and
# approval from maintainer(s).
# minimal requirements

# See comments in https://invent.kde.org/utilities/konsole/-/commit/9d8e47298c81fc1e47c998eda1b6e980589274eb
cmake_minimum_required(VERSION 3.16 FATAL_ERROR)

# Match KDE Frameworks update Apr 2021
set (QT_MIN_VERSION "5.15.0")

set (KF_MIN_VERSION "5.71.0")

# Release script will create bugzilla versions
project(konsole VERSION ${RELEASE_SERVICE_VERSION})

find_package(ECM ${KF_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMOptionalAddSubdirectory)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMMarkNonGuiExecutable)
include(ECMGenerateHeaders)
include(GenerateExportHeader)
include(FeatureSummary)
include(ECMQtDeclareLoggingCategory)
include(KDEClangFormat)
# Remove OPTIONAL when we require EMC >= 5.79
include(KDEGitCommitHooks OPTIONAL)
include(CheckFunctionExists)
include(CheckIncludeFiles)

# Allows passing e.g. -DECM_ENABLE_SANITIZERS='address;undefined' to cmake.
include(ECMEnableSanitizers)

if(ECM_GLOBAL_FIND_VERSION VERSION_LESS "5.82.0")
    if(NOT QT_MAJOR_VERSION)
        set(QT_MAJOR_VERSION "5")
    endif()
endif()

if(QT_MAJOR_VERSION STREQUAL "6")
    find_package(Qt6Core5Compat ${QT_MIN_VERSION} CONFIG REQUIRED)
endif()

ecm_setup_version(${RELEASE_SERVICE_VERSION} VARIABLE_PREFIX KONSOLEPRIVATE
    SOVERSION 1
)
find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED
    Core
    DBus
    Multimedia
    PrintSupport
    Widgets
)

find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED
    Bookmarks
    Config
    ConfigWidgets
    CoreAddons
    Crash
    GuiAddons
    DBusAddons
    I18n
    IconThemes
    KIO
    NewStuff
    Notifications
    NotifyConfig
    Parts
    Service
    TextWidgets
    WidgetsAddons
    WindowSystem
    XmlGui
)

if (NOT WIN32)
    find_package(KF${QT_MAJOR_VERSION} ${KF_MIN_VERSION} REQUIRED
        GlobalAccel
        Pty
    )
endif()

find_package(KF${QT_MAJOR_VERSION}DocTools ${KF_MIN_VERSION})
set_package_properties(KF${QT_MAJOR_VERSION}DocTools PROPERTIES DESCRIPTION
    "Tools to generate documentation"
    TYPE OPTIONAL
)

find_package(ICU 61.0 COMPONENTS uc i18n REQUIRED)

if(NOT APPLE)
    option(WITHOUT_X11 "Build without X11 integration (skips finding X11)" OFF)
    if (NOT WITHOUT_X11)
        find_package(X11)
        set_package_properties(X11 PROPERTIES TYPE OPTIONAL)
        set(HAVE_X11 ${X11_FOUND})
    endif()
endif()

# Check for function GETPWUID
check_symbol_exists(getpwuid "pwd.h" HAVE_GETPWUID)

check_function_exists(malloc_trim HAVE_MALLOC_TRIM)

# See above includes for defaults
add_definitions(
  -DQT_NO_FOREACH
  -DQT_STRICT_ITERATORS
  -DQT_NO_URL_CAST_FROM_STRING
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} )

file(GLOB ICONS_SRCS "data/icons/*.png")

option(ENABLE_PLUGIN_SSHMANAGER "Build the SSHManager plugin" ON)
option(ENABLE_PLUGIN_QUICKCOMMANDS "Build the Quick Commands plugin" ON)

add_subdirectory( src )
add_subdirectory( data )
add_subdirectory( desktop )

if (NOT WIN32)
add_subdirectory( kconf_update )
endif()

if (KF${QT_MAJOR_VERSION}DocTools_FOUND)
    add_subdirectory( doc/manual )
endif()

add_subdirectory( tools )

# Conditionally install icons for Linux as they may not be provided by the user theme
option(INSTALL_ICONS "Install icons" OFF)
if (INSTALL_ICONS)
    include(ECMInstallIcons)
    ecm_install_icons( ICONS ${ICONS_SRCS} DESTINATION ${KDE_INSTALL_ICONDIR} )
endif()

ecm_qt_install_logging_categories(
    EXPORT KONSOLE
    FILE konsole.categories
    DESTINATION "${KDE_INSTALL_LOGGINGCATEGORIESDIR}"
)

ki18n_install( po )
if (KF${QT_MAJOR_VERSION}DocTools_FOUND)
    kdoctools_install( po )
endif()

if (ECM_VERSION VERSION_GREATER_EQUAL 5.101.0)
  install(FILES completions/konsole.zsh RENAME _konsole DESTINATION ${KDE_INSTALL_ZSHAUTOCOMPLETEDIR})
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)

# add clang-format target for all our real source files
file(GLOB_RECURSE ALL_CLANG_FORMAT_SOURCE_FILES *.c *.cpp *.h *.hpp)
kde_clang_format(${ALL_CLANG_FORMAT_SOURCE_FILES})
if (ECM_VERSION VERSION_GREATER_EQUAL 5.79.0)
    kde_configure_git_pre_commit_hook(CHECKS CLANG_FORMAT)
endif()
