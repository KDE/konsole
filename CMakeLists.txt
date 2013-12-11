cmake_minimum_required(VERSION 2.8.12)

project(Konsole)

include(FeatureSummary)
find_package(Qt5Core NO_MODULE)
set_package_properties(Qt5Core PROPERTIES TYPE REQUIRED)
message(STATUS "Qt5 build")
find_package(ECM 0.0.9 NO_MODULE)
set_package_properties(ECM PROPERTIES TYPE REQUIRED)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)

find_package(KF5 CONFIG REQUIRED
    DBusAddons CoreAddons Config
    Codecs I18n Service WidgetsAddons
    WindowSystem Crash Sonnet
    Archive ItemModels Auth GuiAddons ConfigWidgets
    ItemViews Notifications JS Pty
    JobWidgets IconThemes Completion TextWidgets
    GlobalAccel XmlGui Solid Wallet Bookmarks KIO UnitConversion
    NotifyConfig Parts Kross
)
find_package(KF5KDE4Support NO_MODULE)
set_package_properties(KDE4Support PROPERTIES TYPE REQUIRED)
find_package(Qt5Transitional MODULE)
set_package_properties(Qt5Transitional PROPERTIES TYPE REQUIRED)
remove_definitions(-DQT_NO_SIGNALS_SLOTS_KEYWORDS -DQT_NO_CAST_FROM_ASCII)
add_definitions(-Wno-deprecated-declarations)
set(KDE4_KIO_LIBS ${KDE4_KIO_LIBS} ${KDE4Support_LIBRARIES})

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
