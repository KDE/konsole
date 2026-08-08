add_library(konsolecolorscheme STATIC)

# Needed to link this static lib to shared libs
set_target_properties(konsolecolorscheme PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(konsolecolorscheme PRIVATE
    hsluv.c
    ColorScheme.cpp
    ColorSchemeManager.cpp
    ColorSchemeWallpaper.cpp
    ColorSchemeEditor.cpp
    ColorSchemeViewDelegate.cpp
    RandomizationRange.cpp
)

ki18n_wrap_ui(konsolecolorscheme
    ColorSchemeEditor.ui
)

ecm_qt_declare_logging_category(konsolecolorscheme
    HEADER colorschemedebug.h
    IDENTIFIER ColorSchemeDebug
    CATEGORY_NAME org.kde.konsole.colorScheme
    DESCRIPTION "Konsole - colorscheme"
    EXPORT KONSOLE
)

target_link_libraries(konsolecolorscheme
    PUBLIC
        Qt::Widgets
        #TODO: using "CharacterColor.h" frm konsolecharacter in ColorSchemeWallpaper.h
    PRIVATE
        KF6::ConfigCore
        KF6::I18n
        KF6::WidgetsAddons
)
