

# qdbuscpp2xml -m  Session.h -o org.kde.konsole.Session.xml
# Generate dbus .xml files; do not store .xml in source folder
qt5_generate_dbus_interface(Session.h org.kde.konsole.Session.xml OPTIONS -m)

qt5_add_dbus_adaptor(
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
    STATIC
    ${konsole_session_SRCS}
)

target_link_libraries(
    konsolesession
    ${konsole_LIBS}
    konsoleprofile  # TODO: Must be removed from here, Used only for ProfileCommandParser::parse in SessionManager::sessionProfileCommandReceived
)
