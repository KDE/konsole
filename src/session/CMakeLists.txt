#TODO: This Needs TerminalDisplay. Perhaps we should create a library
# that has only TerminalDisplay and link with this one? It's a hard thing
# to do as currently SessionController depends on TerminalDisplay and
# TerminalDisplay depends on SessionController.

# qdbuscpp2xml -m  Session.h -o org.kde.konsole.Session.xml
# Generate dbus .xml files; do not store .xml in source folder
qt_generate_dbus_interface(Session.h org.kde.konsole.Session.xml OPTIONS -m)

qt_add_dbus_adaptor(
    sessionadaptors_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Session.xml
    Session.h
    Konsole::Session
)

set(konsole_session_SRCS
    ${sessionadaptors_SRCS}
    Session.cpp
    SessionController.cpp
    SessionDisplayConnection.cpp
    SessionGroup.cpp
    SessionListModel.cpp
    SessionManager.cpp
    SessionTask.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.konsole.Session.xml
)

add_library(konsolesession
    OBJECT
    ${konsole_session_SRCS}
)

set_property(TARGET konsolesession PROPERTY POSITION_INDEPENDENT_CODE ON)

generate_export_header(konsolesession BASE_NAME konsolesession)
target_include_directories(konsolesession PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(
    konsolesession
    ${konsole_LIBS}
    konsoleprofile  # TODO: Must be removed from here, Used only for ProfileCommandParser::parse in SessionManager::sessionProfileCommandReceived
)
