project(Konsole)

# set_package_properties appeared in cmake 2.8.6
# TODO: Remove when kdelibs >= 4.10 will be required
cmake_minimum_required(VERSION 2.8.6 FATAL_ERROR)

find_package(KDE4 4.7.0 REQUIRED)

include(KDE4Defaults)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
add_definitions(-DQT_USE_FAST_CONCATENATION -DQT_USE_FAST_OPERATOR_PLUS)
add_definitions(-DKDE_DEFAULT_DEBUG_AREA=1211)
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

kde4_no_enable_final(konsole)

add_subdirectory( src )
add_subdirectory( data )
add_subdirectory( desktop )
add_subdirectory( doc/manual )

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
