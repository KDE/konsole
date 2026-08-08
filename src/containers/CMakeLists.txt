# Container detection and management for Konsole
# Provides support for Toolbox, Distrobox, and other container types

add_library(konsolecontainers STATIC)

set_target_properties(konsolecontainers PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(konsolecontainers PRIVATE
    ContainerList.cpp
    ContainerRegistry.cpp
    DistroboxListParser.cpp
    IContainerDetector.cpp
    ToolboxListParser.cpp
    ToolboxDetector.cpp
    DistroboxDetector.cpp
)

if(HAVE_KAPSULE)
    target_sources(konsolecontainers PRIVATE KapsuleDetector.cpp)
endif()

# Some of the konsolecontainers API needs to be exported once linked into konsoleprivate,
# for that the export header of konsoleprivate is reused,
# which needs here setting the export flag or static build flag manually
if(BUILD_SHARED_LIBS)
    target_compile_definitions(konsolecontainers PRIVATE konsoleprivate_EXPORTS)
else()
    target_compile_definitions(konsolecontainers PRIVATE KONSOLEPRIVATE_STATIC_DEFINE)
endif()

target_link_libraries(konsolecontainers
    PUBLIC
        Qt::Core
    PRIVATE
        konsolelogging
        KF6::I18n
        KF6::CoreAddons  # For KSandbox
        Qt::Widgets
)

if(HAVE_KAPSULE)
    target_link_libraries(konsolecontainers PRIVATE Kapsule::KapsuleQt)
endif()

target_include_directories(konsolecontainers
    PRIVATE
    ${CMAKE_BINARY_DIR}/src  # For config-konsole.h
)
