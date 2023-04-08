include(ECMAddTests)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt::Test konsoleprivate konsolecharacters konsoledecoders)

# ATM issue with non-termination due to message dialog
if(QT_MAJOR_VERSION EQUAL "5")
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
        ecm_add_test(DBusTest.cpp LINK_LIBRARIES ${KONSOLE_TEST_LIBS} Qt::DBus)
    endif()
endif()
endif()

# Temporary bypass for BKO 432379
if(BUILD_SHARED_LIBS)
    ecm_add_test(KeyboardTranslatorTest.cpp LINK_LIBRARIES ${KONSOLE_TEST_LIBS})
endif(BUILD_SHARED_LIBS)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    ecm_add_test(
        PartTest.cpp
        LINK_LIBRARIES KF${QT_MAJOR_VERSION}::XmlGui KF${QT_MAJOR_VERSION}::Parts KF${QT_MAJOR_VERSION}::Pty ${KONSOLE_TEST_LIBS}
    )
endif()

ecm_add_tests(
    BookMarkTest.cpp
    CharacterColorTest.cpp
    CharacterTest.cpp
    CharacterWidthTest.cpp
    HotSpotFilterTest.cpp
    ProfileTest.cpp
    PtyTest.cpp
    ScreenTest.cpp
    ShellCommandTest.cpp
    TerminalCharacterDecoderTest.cpp
    Vt102EmulationTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS}
)
target_link_libraries(PtyTest KF${QT_MAJOR_VERSION}::Pty ${KONSOLE_TEST_LIBS})

ecm_add_tests(
    HistoryTest.cpp
    SessionTest.cpp
    TerminalInterfaceTest.cpp
    TerminalTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS} KF${QT_MAJOR_VERSION}::Parts
)
