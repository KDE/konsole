
### konsoleprofile command-line tool
install(PROGRAMS konsoleprofile DESTINATION ${KDE_INSTALL_BINDIR})

### Line graphics font
###  Attempting to auto-create LineFont.h for multiple systems is a headache.
###  If LineFont.h is needed to be recreated use:
###     fontembedder LineFont.src > LineFont.h
###  Then commit the new LineFont.h
if(KONSOLE_BUILD_FONTEMBEDDER OR KONSOLE_GENERATE_LINEFONT)

    find_package(Qt5Core ${QT_MIN_VERSION} CONFIG REQUIRED)

    ### Font Embedder
    set(fontembedder_SRCS fontembedder.cpp)
    add_executable(fontembedder ${fontembedder_SRCS})
    target_link_libraries(fontembedder Qt5::Core)
endif()

add_subdirectory( uni2characterwidth )
