include(ECMAddTests)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt::Test konsoleprivate konsolecharacters konsoledecoders)

# Temporary bypass for BKO 432379
if(BUILD_SHARED_LIBS)
    ecm_add_test(KeyboardTranslatorTest.cpp LINK_LIBRARIES ${KONSOLE_TEST_LIBS})
endif(BUILD_SHARED_LIBS)

if (NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT WIN32)
    ecm_add_test(
        PartTest.cpp
        LINK_LIBRARIES KF6::XmlGui KF6::Parts KF6::Pty ${KONSOLE_TEST_LIBS}
    )
endif()

ecm_add_tests(
    BookMarkTest.cpp
    CharacterColorTest.cpp
    CharacterTest.cpp
    CharacterWidthTest.cpp
    HotSpotFilterTest.cpp
    ProcessInfoTest.cpp
    ProfileTest.cpp
    ScreenTest.cpp
    ShellCommandTest.cpp
    TerminalCharacterDecoderTest.cpp
    Vt102EmulationTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS}
)

if (NOT WIN32)
    ecm_add_tests(
        PtyTest.cpp
        LINK_LIBRARIES KF6::Pty ${KONSOLE_TEST_LIBS}
    )
endif()

ecm_add_tests(
    HistoryTest.cpp
    SessionTest.cpp
    TerminalInterfaceTest.cpp
    TerminalTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS} KF6::Parts
)
