include(ECMMarkAsTest)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt5::Test konsoleprivate)

add_executable(PartManualTest PartManualTest.cpp)
ecm_mark_as_test(PartManualTest)
target_link_libraries(PartManualTest KF5::XmlGui KF5::Parts KF5::Pty
                     ${KONSOLE_TEST_LIBS})

kde4_add_unit_test(TerminalInterfaceTest TerminalInterfaceTest.cpp)
target_link_libraries(TerminalInterfaceTest
                      ${KDE4_KPARTS_LIBS}
                      ${KDE4_KPTY_LIBS}
                      ${KONSOLE_TEST_LIBS})
