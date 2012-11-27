set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${KDE4_KPTY_INCLUDES} ${KDE4_KPARTS_INCLUDES} ${QT_QTTEST_INCLUDES})

set(KONSOLE_TEST_LIBS ${QT_QTTEST_LIBRARY} konsoleprivate)

kde4_add_executable(PartTest TEST PartTest.cpp)
target_link_libraries(PartTest ${KDE4_KPARTS_LIBS} ${KDE4_KPTY_LIBS} ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(PtyTest PtyTest.cpp)
target_link_libraries(PtyTest ${KDE4_KPTY_LIBS} ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(ShellCommandTest ShellCommandTest.cpp)
target_link_libraries(ShellCommandTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(CharacterColorTest CharacterColorTest.cpp)
target_link_libraries(CharacterColorTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(TerminalCharacterDecoderTest TerminalCharacterDecoderTest.cpp)
target_link_libraries(TerminalCharacterDecoderTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(ProfileTest ProfileTest.cpp)
target_link_libraries(ProfileTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(SessionManagerTest SessionManagerTest.cpp)
target_link_libraries(SessionManagerTest ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(DBusTest DBusTest.cpp)
target_link_libraries(DBusTest ${KONSOLE_TEST_LIBS})

