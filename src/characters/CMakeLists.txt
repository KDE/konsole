
set(konsolecharacters_SRCS
    CharacterFormat.cpp
    CharacterWidth.cpp
    Hangul.cpp
    LineBlockCharacters.cpp
    ExtendedCharTable.cpp
)

ecm_qt_declare_logging_category(
    konsolecharacters_SRCS
    HEADER charactersdebug.h
    IDENTIFIER CharactersDebug
    CATEGORY_NAME org.kde.konsole.characters
    DESCRIPTION "Konsole - characters"
    EXPORT KONSOLE
)

add_library(konsolecharacters OBJECT ${konsolecharacters_SRCS})
generate_export_header(konsolecharacters BASE_NAME konsolecharacters)
target_include_directories(konsolecharacters
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(konsolecharacters Qt5::Gui)
