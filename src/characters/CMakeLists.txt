add_library(konsolecharacters OBJECT)

# Needed to link this static lib to shared libs
set_target_properties(konsolecharacters PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(konsolecharacters PRIVATE
    CharacterWidth.cpp
    Hangul.cpp
    LineBlockCharacters.cpp
    ExtendedCharTable.cpp
)

ecm_qt_declare_logging_category(konsolecharacters
    HEADER charactersdebug.h
    IDENTIFIER CharactersDebug
    CATEGORY_NAME org.kde.konsole.characters
    DESCRIPTION "Konsole - characters"
    EXPORT KONSOLE
)

target_include_directories(konsolecharacters
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(konsolecharacters PUBLIC Qt::Gui)
