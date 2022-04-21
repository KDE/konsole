include(ECMMarkAsTest)
include(ECMMarkNonGuiExecutable)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt::Test konsoleprivate)

add_executable(BookMarkTest BookMarkTest.cpp)
ecm_mark_as_test(BookMarkTest)
ecm_mark_nongui_executable(BookMarkTest)
add_test(NAME BookMarkTest COMMAND BookMarkTest)
target_link_libraries(BookMarkTest ${KONSOLE_TEST_LIBS})

add_executable(CharacterColorTest CharacterColorTest.cpp)
ecm_mark_as_test(CharacterColorTest)
ecm_mark_nongui_executable(CharacterColorTest)
add_test(NAME CharacterColorTest COMMAND CharacterColorTest)
target_link_libraries(CharacterColorTest ${KONSOLE_TEST_LIBS})

add_executable(CharacterWidthTest CharacterWidthTest.cpp)
ecm_mark_as_test(CharacterWidthTest)
ecm_mark_nongui_executable(CharacterWidthTest)
add_test(NAME CharacterWidthTest COMMAND CharacterWidthTest)
target_link_libraries(CharacterWidthTest ${KONSOLE_TEST_LIBS})

add_executable(CharacterTest CharacterTest.cpp)
ecm_mark_as_test(CharacterTest)
ecm_mark_nongui_executable(CharacterTest)
add_test(NAME CharacterTest COMMAND CharacterTest)
target_link_libraries(CharacterTest ${KONSOLE_TEST_LIBS})

if ("$ENV{USER}" STREQUAL "jenkins")
    message(STATUS "We are running in jenkins; skipping DBusTest...")
else()
    message(STATUS "We are NOT running in jenkins; running DBusTest...")

    # This test fails on kf5-qt5 SUSEQt5.9 buildbot since Oct 28, 2018
    # Believed due to frameworks regression; disable to avoid sysadmins having
    # to fix for every build
    # https://mail.kde.org/pipermail/kde-frameworks-devel/2018-November/073078.html
    # https://phabricator.kde.org/T9948
    if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
        add_executable(DBusTest DBusTest.cpp)
        ecm_mark_as_test(DBusTest)
        ecm_mark_nongui_executable(DBusTest)
        add_test(NAME DBusTest COMMAND DBusTest)
        target_link_libraries(DBusTest ${KONSOLE_TEST_LIBS} Qt::DBus)
    endif()
endif()

add_executable(HistoryTest HistoryTest.cpp)
ecm_mark_as_test(HistoryTest)
ecm_mark_nongui_executable(HistoryTest)
add_test(NAME HistoryTest COMMAND HistoryTest)
target_link_libraries(HistoryTest ${KONSOLE_TEST_LIBS} KF5::Parts)

# Temporary bypass for BKO 432379
if(BUILD_SHARED_LIBS)
add_executable(KeyboardTranslatorTest KeyboardTranslatorTest.cpp)
ecm_mark_as_test(KeyboardTranslatorTest)
ecm_mark_nongui_executable(KeyboardTranslatorTest)
add_test(NAME KeyboardTranslatorTest COMMAND KeyboardTranslatorTest)
target_link_libraries(KeyboardTranslatorTest ${KONSOLE_TEST_LIBS})
endif(BUILD_SHARED_LIBS)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    add_executable(PartTest PartTest.cpp)
    ecm_mark_as_test(PartTest)
    ecm_mark_nongui_executable(PartTest)
    add_test(NAME PartTest COMMAND PartTest)
    target_link_libraries(PartTest KF5::XmlGui KF5::Parts KF5::Pty
                               ${KONSOLE_TEST_LIBS})
endif()

add_executable(ProfileTest ProfileTest.cpp)
ecm_mark_as_test(ProfileTest)
ecm_mark_nongui_executable(ProfileTest)
add_test(NAME ProfileTest COMMAND ProfileTest)
target_link_libraries(ProfileTest ${KONSOLE_TEST_LIBS})

add_executable(PtyTest PtyTest.cpp)
ecm_mark_as_test(PtyTest)
ecm_mark_nongui_executable(PtyTest)
add_test(NAME PtyTest COMMAND PtyTest)
target_link_libraries(PtyTest KF5::Pty ${KONSOLE_TEST_LIBS})

add_executable(SessionTest SessionTest.cpp)
ecm_mark_as_test(SessionTest)
ecm_mark_nongui_executable(SessionTest)
add_test(NAME SessionTest COMMAND SessionTest)
target_link_libraries(SessionTest ${KONSOLE_TEST_LIBS} KF5::Parts)

add_executable(ShellCommandTest ShellCommandTest.cpp)
ecm_mark_as_test(ShellCommandTest)
ecm_mark_nongui_executable(ShellCommandTest)
add_test(NAME ShellCommandTest COMMAND ShellCommandTest)
target_link_libraries(ShellCommandTest ${KONSOLE_TEST_LIBS})

add_executable(ScreenTest ScreenTest.cpp)
ecm_mark_as_test(ScreenTest)
ecm_mark_nongui_executable(ScreenTest)
add_test(NAME ScreenTest COMMAND ScreenTest)
target_link_libraries(ScreenTest ${KONSOLE_TEST_LIBS})

add_executable(TerminalCharacterDecoderTest
               TerminalCharacterDecoderTest.cpp)
ecm_mark_as_test(TerminalCharacterDecoderTest)
ecm_mark_nongui_executable(TerminalCharacterDecoderTest)
add_test(NAME TerminalCharacterDecoderTest COMMAND TerminalCharacterDecoderTest)
target_link_libraries(TerminalCharacterDecoderTest ${KONSOLE_TEST_LIBS})

add_executable(TerminalTest TerminalTest.cpp)
ecm_mark_as_test(TerminalTest)
ecm_mark_nongui_executable(TerminalTest)
add_test(NAME TerminalTest COMMAND TerminalTest)
target_link_libraries(TerminalTest ${KONSOLE_TEST_LIBS} KF5::Parts)

add_executable(TerminalInterfaceTest TerminalInterfaceTest.cpp)
ecm_mark_as_test(TerminalInterface)
ecm_mark_nongui_executable(TerminalInterfaceTest)
add_test(NAME TerminalInterfaceTest COMMAND TerminalInterfaceTest)
target_link_libraries(TerminalInterfaceTest
                      KF5::Parts
                      ${KONSOLE_TEST_LIBS})

add_executable(Vt102EmulationTest Vt102EmulationTest.cpp)
ecm_mark_as_test(Vt102EmulationTest)
ecm_mark_nongui_executable(Vt102EmulationTest)
add_test(NAME Vt102EmulationTest COMMAND Vt102EmulationTest)
target_link_libraries(Vt102EmulationTest ${KONSOLE_TEST_LIBS})

include(ECMAddTests)
ecm_add_test(
    HotSpotFilterTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS}
)
