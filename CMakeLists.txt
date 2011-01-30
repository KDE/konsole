project(Konsole)

if(WIN32)
    set(REQ_VERSION 4.5.0)
else(WIN32)
    set(REQ_VERSION 4.4.60)
endif(WIN32)
find_package(KDE4 ${REQ_VERSION} REQUIRED)

include(KDE4Defaults)

add_definitions(${QT_DEFINITIONS} ${KDE4_DEFINITIONS})
include_directories(${CMAKE_SOURCE_DIR} ${CMAKE_BINARY_DIR} ${KDE4_INCLUDES})

KDE4_NO_ENABLE_FINAL(konsole)

add_subdirectory( src ) 
add_subdirectory( data ) 
add_subdirectory( desktop )


