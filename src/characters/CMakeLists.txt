
set(konsolecharacters_SRCS
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
# Needed to link this static lib to shared libs
set_target_properties(konsolecharacters PROPERTIES POSITION_INDEPENDENT_CODE ON)
generate_export_header(konsolecharacters BASE_NAME konsolecharacters)
target_include_directories(konsolecharacters
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(konsolecharacters Qt::Gui)
