
set(konsole_colorscheme_SRCS
    hsluv.c
    ColorScheme.cpp
    ColorSchemeManager.cpp
    ColorSchemeWallpaper.cpp
    ColorSchemeEditor.cpp
    ColorSchemeViewDelegate.cpp
    RandomizationRange.cpp
)

ki18n_wrap_ui(konsole_colorscheme_SRCS
    ColorSchemeEditor.ui
)

ecm_qt_declare_logging_category(
    konsole_colorscheme_SRCS
    HEADER colorschemedebug.h
    IDENTIFIER ColorSchemeDebug
    CATEGORY_NAME org.kde.konsole.colorScheme
    DESCRIPTION "Konsole - colorscheme"
    EXPORT KONSOLE
)

add_library(konsolecolorscheme
    STATIC
    ${konsole_colorscheme_SRCS}
)
generate_export_header(konsolecolorscheme BASE_NAME konsolecolorscheme)
target_include_directories(konsolecolorscheme
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(
    konsolecolorscheme
    ${konsole_LIBS}
)
