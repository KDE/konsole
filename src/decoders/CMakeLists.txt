
add_library(konsoledecoders
    OBJECT
    PlainTextDecoder.cpp
    HTMLDecoder.cpp
)

set_target_properties(konsoledecoders PROPERTIES POSITION_INDEPENDENT_CODE ON)
generate_export_header(konsoledecoders BASE_NAME konsoledecoders)
target_include_directories(konsoledecoders
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# TODO: remove the konsolecolorscheme dependencies
target_link_libraries(konsoledecoders konsolecharacters konsolecolorscheme)
