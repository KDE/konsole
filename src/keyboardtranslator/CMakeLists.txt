add_library(keyboardtranslator STATIC)

# Needed to link this static lib to shared libs
set_target_properties(keyboardtranslator PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(keyboardtranslator PRIVATE
    KeyboardTranslatorManager.cpp
    KeyboardTranslatorWriter.cpp
    KeyboardTranslator.cpp
    KeyboardTranslatorReader.cpp
)

target_link_libraries(keyboardtranslator
    PUBLIC
        Qt::Core
    PRIVATE
        KF6::I18n
        Qt::Gui
)
