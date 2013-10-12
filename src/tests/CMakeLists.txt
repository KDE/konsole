set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${KDE4_KPTY_INCLUDES}
                   ${KDE4_KPARTS_INCLUDES}
                   ${QT_QTTEST_INCLUDES})

set(KONSOLE_TEST_LIBS Qt5::Test konsoleprivate)

## If it's interactive, it shouldn't use kde4_add_unit_test,
##  since make test is supposed to be fully automated.

kde4_add_executable(PartManualTest TEST PartManualTest.cpp)
target_link_libraries(PartManualTest XmlGui KParts
                               ${KDE4_KPTY_LIBS}
                               ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(CharacterColorTest CharacterColorTest.cpp)
target_link_libraries(CharacterColorTest ${KONSOLE_TEST_LIBS})

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    kde4_add_unit_test(DBusTest DBusTest.cpp)
    target_link_libraries(DBusTest ${KONSOLE_TEST_LIBS} Qt5::DBus)
endif()

kde4_add_unit_test(HistoryTest HistoryTest.cpp)
target_link_libraries(HistoryTest ${KONSOLE_TEST_LIBS})

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    kde4_add_unit_test(PartTest PartTest.cpp)
    target_link_libraries(PartTest XmlGui KParts
                               ${KDE4_KPTY_LIBS}
                               ${KONSOLE_TEST_LIBS})
endif()

kde4_add_unit_test(ProfileTest ProfileTest.cpp)
target_link_libraries(ProfileTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(PtyTest PtyTest.cpp)
target_link_libraries(PtyTest ${KDE4_KPTY_LIBS} ${KONSOLE_TEST_LIBS})

##kde4_add_unit_test(SessionManagerTest SessionManagerTest.cpp)
##target_link_libraries(SessionManagerTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(SessionTest SessionTest.cpp)
target_link_libraries(SessionTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(ShellCommandTest ShellCommandTest.cpp)
target_link_libraries(ShellCommandTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(TerminalCharacterDecoderTest
                  TerminalCharacterDecoderTest.cpp)
target_link_libraries(TerminalCharacterDecoderTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(TerminalTest TerminalTest.cpp)
target_link_libraries(TerminalTest ${KONSOLE_TEST_LIBS})

