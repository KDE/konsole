include(ECMAddTests)

if(KONSOLE_HAVE_XKBCOMMON)
    add_compile_definitions(HAVE_XKBCOMMON)

    list(APPEND KONSOLE_TEST_LIBS ${XKBCOMMON_LIBRARIES})
endif()

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt::Test konsoleprivate konsolecharacters konsoledecoders)

if (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    set(KONSOLE_TEST_LIBS ${KONSOLE_TEST_LIBS} kvm)
endif()

# Temporary bypass for BKO 432379
if(BUILD_SHARED_LIBS)
    ecm_add_test(KeyboardTranslatorTest.cpp LINK_LIBRARIES ${KONSOLE_TEST_LIBS})
endif()

if(BUILD_KPART AND NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT WIN32)
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
    ContainerDetectorParsingTest.cpp
    HotSpotFilterTest.cpp
    ProcessInfoTest.cpp
    ProfileTest.cpp
    ScreenTest.cpp
    ShellCommandTest.cpp
    TerminalCharacterDecoderTest.cpp
    Vt102EmulationTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS} konsolecontainers
)

if(NOT WIN32)
    ecm_add_tests(
        PtyTest.cpp
        LINK_LIBRARIES KF6::Pty ${KONSOLE_TEST_LIBS}
    )
endif()

ecm_add_tests(
    HistoryTest.cpp
    SessionTest.cpp
    TerminalTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS} KF6::Parts
)

if(BUILD_KPART)
ecm_add_tests(
    TerminalInterfaceTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS} KF6::Parts
)
endif()

if(BUILD_APP)
ecm_add_test(
    ViewManagerTest.cpp
    LINK_LIBRARIES ${KONSOLE_TEST_LIBS} konsoleapp KF6::Parts
)
endif()
