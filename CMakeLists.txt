project(Konsole)

find_package(KDE4 4.5.40 REQUIRED)

include(KDE4Defaults)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

KDE4_NO_ENABLE_FINAL(konsole)

add_subdirectory( src ) 
add_subdirectory( data ) 
add_subdirectory( desktop )


