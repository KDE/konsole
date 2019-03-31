# Konsole project

# KDE Application Version, managed by release script
set (KDE_APPLICATIONS_VERSION_MAJOR "19")
set (KDE_APPLICATIONS_VERSION_MINOR "03")
set (KDE_APPLICATIONS_VERSION_MICRO "90")
set (KDE_APPLICATIONS_VERSION "${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}.${KDE_APPLICATIONS_VERSION_MICRO}")

# minimal requirements
cmake_minimum_required (VERSION 3.0 FATAL_ERROR)
# Qt 5.9 LTS EOL May 2020
set (QT_MIN_VERSION "5.9.0")

set (KF5_MIN_VERSION "5.6.0")

# Release script will create bugzilla versions
project(konsole VERSION ${KDE_APPLICATIONS_VERSION})

find_package(ECM 1.6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(ECMOptionalAddSubdirectory)
include(ECMInstallIcons)
include(ECMSetupVersion)
include(ECMMarkNonGuiExecutable)
include(ECMGenerateHeaders)
include(GenerateExportHeader)
include(FeatureSummary)
include(ECMQtDeclareLoggingCategory)

ecm_setup_version(${KDE_APPLICATIONS_VERSION} VARIABLE_PREFIX KONSOLEPRIVATE
    SOVERSION ${KDE_APPLICATIONS_VERSION_MAJOR}
)
find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED
    Core
    DBus
    PrintSupport
    Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED
    Bookmarks Completion Config ConfigWidgets
    CoreAddons Crash GuiAddons DBusAddons
    I18n IconThemes Init KIO NewStuff NewStuffCore Notifications NotifyConfig
    Parts Pty Service TextWidgets WidgetsAddons
    WindowSystem XmlGui DBusAddons GlobalAccel
)

find_package(KF5DocTools ${KF5_MIN_VERSION})
set_package_properties(KF5DocTools PROPERTIES DESCRIPTION 
    "Tools to generate documentation"
    TYPE OPTIONAL
)

if(NOT APPLE)
    find_package(X11)
    set_package_properties(X11 PROPERTIES TYPE OPTIONAL)
endif()
set(HAVE_X11 ${X11_FOUND})

# FIXME: Fix code to allow this
#add_definitions(-DQT_NO_FOREACH)

# See above includes for defaults
add_definitions(
  -DQT_STRICT_ITERATORS
  -DQT_NO_URL_CAST_FROM_STRING
)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} )

add_subdirectory( src )
add_subdirectory( data )
add_subdirectory( desktop )
if (KF5DocTools_FOUND)
    add_subdirectory( doc/manual )
endif()

add_subdirectory( tools )

install( FILES konsole.categories DESTINATION ${KDE_INSTALL_CONFDIR} )

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
