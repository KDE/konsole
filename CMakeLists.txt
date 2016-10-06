# Konsole project
project(Konsole)

# KDE Application Version, managed by release script
set (KDE_APPLICATIONS_VERSION_MAJOR "16")
set (KDE_APPLICATIONS_VERSION_MINOR "08")
set (KDE_APPLICATIONS_VERSION_MICRO "2")
set (KDE_APPLICATIONS_VERSION "${KDE_APPLICATIONS_VERSION_MAJOR}.${KDE_APPLICATIONS_VERSION_MINOR}.${KDE_APPLICATIONS_VERSION_MICRO}")

# minimal requirements
cmake_minimum_required (VERSION 2.8.12 FATAL_ERROR)
set (QT_MIN_VERSION "5.2.0")
set (KF5_MIN_VERSION "5.6.0")

find_package(ECM 1.6.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${ECM_MODULE_PATH} ${ECM_KDE_MODULE_DIR})

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

ecm_setup_version(${KDE_APPLICATIONS_VERSION} VARIABLE_PREFIX KONSOLEPRIVATE
    SOVERSION ${KDE_APPLICATIONS_VERSION_MAJOR}
)
find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED
    Core
    DBus
    PrintSupport
    Script
    Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED
    Bookmarks Completion Config ConfigWidgets
    CoreAddons Crash GuiAddons DBusAddons
    I18n IconThemes Init KIO Notifications NotifyConfig
    Parts Pty Service TextWidgets WidgetsAddons
    WindowSystem XmlGui DBusAddons
)

find_package(KF5DocTools ${KF5_MIN_VERSION})
set_package_properties(KF5DocTools PROPERTIES DESCRIPTION 
    "Tools to generate documentation"
    TYPE OPTIONAL
)

find_package(X11)
set_package_properties(X11 PROPERTIES TYPE OPTIONAL)
set(HAVE_X11 ${X11_FOUND})

add_definitions(
  -DQT_USE_QSTRINGBUILDER
  -DQT_NO_CAST_TO_ASCII
#  -DQT_NO_CAST_FROM_ASCII
  -DQT_STRICT_ITERATORS
  -DQT_NO_URL_CAST_FROM_STRING
  -DQT_NO_CAST_FROM_BYTEARRAY
#  -DQT_NO_SIGNALS_SLOTS_KEYWORDS
  -DQT_USE_FAST_OPERATOR_PLUS
  -DQT_USE_FAST_CONCATENATION
  -Wno-deprecated-declarations
)

include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} )

add_subdirectory( src )
add_subdirectory( data )
add_subdirectory( desktop )
if (KF5DocTools_FOUND)
    add_subdirectory( doc/manual )
endif()

### -DKONSOLE_BUILD_FONTEMBEDDER=TRUE or -DKONSOLE_GENERATE_LINEFONT=TRUE
if(KONSOLE_BUILD_FONTEMBEDDER OR KONSOLE_GENERATE_LINEFONT)
    add_subdirectory( tools )
endif()

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
