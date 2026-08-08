add_library(konsoledecoders OBJECT)

set_target_properties(konsoledecoders PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(konsoledecoders PRIVATE
    PlainTextDecoder.cpp
    HTMLDecoder.cpp
)

target_include_directories(konsoledecoders
    PUBLIC ${CMAKE_CURRENT_BINARY_DIR}
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

# TODO: remove the konsolecolorscheme dependencies
target_link_libraries(konsoledecoders PUBLIC konsolecharacters)
