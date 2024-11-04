cmake_minimum_required(VERSION 3.16)

project(demo_konsolepart)
set(PROJECT_VERSION "1.0.0")

find_package(ECM 1.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMInstallIcons)
include(FeatureSummary)

set(QT_MIN_VERSION "6.5.0")
find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS Core Gui Widgets)

set(REQUIRED_KF_VERSION "6.0.0")
find_package(KF6 ${REQUIRED_KF_VERSION} REQUIRED COMPONENTS
    CoreAddons
    I18n
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

add_subdirectory(src)
