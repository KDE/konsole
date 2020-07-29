add_library(keyboardtranslator
    STATIC
    FallbackKeyboardTranslator.cpp
    KeyboardTranslatorManager.cpp
    KeyboardTranslatorWriter.cpp
    KeyboardTranslator.cpp
    KeyboardTranslatorReader.cpp
)

target_link_libraries(
    keyboardtranslator
    ${konsole_LIBS}
)

