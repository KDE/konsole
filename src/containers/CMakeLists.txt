# Container detection and management for Konsole
# Provides support for Toolbox, Distrobox, and other container types

set(konsolecontainers_SRCS
    ContainerList.cpp
    ContainerRegistry.cpp
    IContainerDetector.cpp
    ToolboxDetector.cpp
    DistroboxDetector.cpp
)

add_library(konsolecontainers STATIC ${konsolecontainers_SRCS})

set_target_properties(konsolecontainers PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_compile_features(konsolecontainers PUBLIC cxx_std_20)

target_link_libraries(konsolecontainers
    PUBLIC
    Qt::Core
    Qt::Widgets
    KF6::I18n
    KF6::CoreAddons  # For KSandbox
)

target_include_directories(konsolecontainers PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
