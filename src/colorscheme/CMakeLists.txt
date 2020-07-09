set(konsole_colorscheme_SRCS
    hsluv.c
    ColorScheme.cpp
    ColorSchemeManager.cpp
    ColorSchemeWallpaper.cpp
    ColorSchemeEditor.cpp
    ColorSchemeViewDelegate.cpp
)

ki18n_wrap_ui(konsole_colorscheme_SRCS
    ColorSchemeEditor.ui
)

add_library(konsolecolorscheme
    STATIC
    ${konsole_colorscheme_SRCS}
)

target_link_libraries(
    konsolecolorscheme
    ${konsole_LIBS}
    konsoleprivate_core
)

