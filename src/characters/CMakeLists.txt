
add_library(konsolecharacters
    OBJECT
    CharacterFormat.cpp
    CharacterWidth.cpp
    LineBlockCharacters.cpp
)
generate_export_header(konsolecharacters BASE_NAME konsolecharacters)
target_include_directories(konsolecharacters PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(konsolecharacters Qt5::Gui)
