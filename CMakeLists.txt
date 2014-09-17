# Konsole project
project(Konsole)
set(Konsole_VERSION_MAJOR 2)
set(Konsole_VERSION_MINOR 99)
set(Konsole_VERSION_PATCH 900)
set(Konsole_VERSION "${Konsole_VERSION_MAJOR}.${Konsole_VERSION_MINOR}.${Konsole_VERSION_PATCH}")

# minimal requirements
cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)
set (QT_MIN_VERSION "5.2.0")

find_package(ECM 1.1.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(ECMOptionalAddSubdirectory)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMMarkNonGuiExecutable)
include(ECMGenerateHeaders)
include(GenerateExportHeader)
include(FeatureSummary)

ecm_setup_version(${Konsole_VERSION} VARIABLE_PREFIX KONSOLEPRIVATE
                  SOVERSION ${Konsole_VERSION_MAJOR}
)
find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED Core DBus Widgets Script Test)

find_package(KF5 REQUIRED
    Bookmarks Completion Config ConfigWidgets
    CoreAddons DocTools GuiAddons I18n IconThemes
    Init KDELibs4Support KIO Notifications NotifyConfig
    Parts Pty Service TextWidgets WidgetsAddons
    WindowSystem XmlGui
)

find_package(X11)
set_package_properties(X11 PROPERTIES TYPE OPTIONAL)
set(HAVE_X11 ${X11_FOUND})

remove_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS -DQT_NO_CAST_FROM_ASCII)
add_definitions(-Wno-deprecated-declarations)

add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
add_definitions(-DKDE_DEFAULT_DEBUG_AREA=1211)
#TODO: this is temporarily disabled until it has been ported to Qt5
add_definitions(-DQT_NO_ACCESSIBILITY)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

add_subdirectory( src )
add_subdirectory( data )
add_subdirectory( desktop )
add_subdirectory( doc/manual )

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
