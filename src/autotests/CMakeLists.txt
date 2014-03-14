include(ECMMarkAsTest)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt5::Test konsoleprivate)

add_executable(CharacterColorTest CharacterColorTest.cpp)
ecm_mark_as_test(CharacterColorTest)
add_test(CharacterColorTest CharacterColorTest)
target_link_libraries(CharacterColorTest ${KONSOLE_TEST_LIBS})

add_executable(CharacterWidthTest CharacterWidthTest.cpp)
ecm_mark_as_test(CharacterWidthTest)
add_test(CharacterWidthTest CharacterWidthTest)
target_link_libraries(CharacterWidthTest ${KONSOLE_TEST_LIBS})


if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_executable(DBusTest DBusTest.cpp)
    ecm_mark_as_test(DBusTest)
    add_test(DBusTest DBusTest)
    target_link_libraries(DBusTest ${KONSOLE_TEST_LIBS} Qt5::DBus)
endif()

add_executable(HistoryTest HistoryTest.cpp)
ecm_mark_as_test(HistoryTest)
add_test(HistoryTest HistoryTest)
target_link_libraries(HistoryTest ${KONSOLE_TEST_LIBS})


add_executable(KeyboardTranslatorTest KeyboardTranslatorTest.cpp)
ecm_mark_as_test(KeyboardTranslatorTest)
add_test(KeyboardTranslatorTest KeyboardTranslatorTest)
target_link_libraries(KeyboardTranslatorTest ${KONSOLE_TEST_LIBS})

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_executable(PartTest PartTest.cpp)
    ecm_mark_as_test(PartTest)
    add_test(PartTest PartTest)
    target_link_libraries(PartTest KF5::XmlGui KF5::Parts KF5::Pty
                               ${KONSOLE_TEST_LIBS})
endif()

add_executable(ProfileTest ProfileTest.cpp)
ecm_mark_as_test(ProfileTest)
add_test(ProfileTest ProfileTest)
target_link_libraries(ProfileTest ${KONSOLE_TEST_LIBS})

add_executable(PtyTest PtyTest.cpp)
ecm_mark_as_test(PtyTest)
add_test(PtyTest PtyTest)
target_link_libraries(PtyTest KF5::Pty ${KONSOLE_TEST_LIBS})

add_executable(SessionTest SessionTest.cpp)
ecm_mark_as_test(SessionTest)
add_test(SessionTest SessionTest)
target_link_libraries(SessionTest ${KONSOLE_TEST_LIBS})

add_executable(ShellCommandTest ShellCommandTest.cpp)
ecm_mark_as_test(ShellCommandTest)
add_test(ShellCommandTest ShellCommandTest)
target_link_libraries(ShellCommandTest ${KONSOLE_TEST_LIBS})

add_executable(TerminalCharacterDecoderTest
               TerminalCharacterDecoderTest.cpp)
ecm_mark_as_test(TerminalCharacterDecoderTest)
add_test(TerminalCharacterDecoderTest TerminalCharacterDecoderTest)
target_link_libraries(TerminalCharacterDecoderTest ${KONSOLE_TEST_LIBS})

add_executable(TerminalTest TerminalTest.cpp)
ecm_mark_as_test(TerminalTest)
add_test(TerminalTest TerminalTest)
target_link_libraries(TerminalTest ${KONSOLE_TEST_LIBS})

##add_executable(TerminalInterfaceTest TerminalInterfaceTest.cpp)
##ecm_mark_as_test(TerminalInterface)
##add_test(TerminalInterfaceTest TerminalInterfaceTest)
##target_link_libraries(TerminalInterfaceTest
##                      ${KDE4_KPARTS_LIBS}
##                      ${KDE4_KPTY_LIBS}
##                      ${KONSOLE_TEST_LIBS})

