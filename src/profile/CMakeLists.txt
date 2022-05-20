

add_library(konsoleprofile
OBJECT
    Profile.cpp
    ProfileCommandParser.cpp
    ProfileGroup.cpp
    ProfileList.cpp
    ProfileReader.cpp
    ProfileWriter.cpp
    ProfileManager.cpp
    ProfileModel.cpp
    ${konsoleprofile_SRCS}
)
set_property(TARGET konsoleprofile PROPERTY POSITION_INDEPENDENT_CODE ON)

generate_export_header(konsoleprofile BASE_NAME konsoleprofile)
target_include_directories(konsoleprofile PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
    konsoleprofile
    ${konsole_LIBS}
    konsoleprivate_core
    konsolecolorscheme
    keyboardtranslator
    konsolehelpers
    KF5::CoreAddons
)
