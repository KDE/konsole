

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

target_link_libraries(
    konsoleprofile
    ${konsole_LIBS}
    konsoleprivate_core
    konsolecolorscheme
    keyboardtranslator
    konsolehelpers
)
