add_library(keyboardtranslator
    STATIC
    FallbackKeyboardTranslator.cpp
    KeyboardTranslatorManager.cpp
    KeyboardTranslatorWriter.cpp
    KeyboardTranslator.cpp
    KeyboardTranslatorReader.cpp
)
# Needed to link this static lib to shared libs
set_target_properties(keyboardtranslator PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_link_libraries(
    keyboardtranslator
    ${konsole_LIBS}
)

