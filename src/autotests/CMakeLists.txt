include(ECMAddTests)

ecm_add_test(
    KeyboardTranslatorTest.cpp
    LINK_LIBRARIES keyboardtranslator Qt::Test
)

if(NOT ${CMAKE_SYSTEM_NAME} MATCHES "Darwin" AND NOT WIN32)
    ecm_add_test(
        PartTest.cpp
        LINK_LIBRARIES konsoleprivate KF6::Parts KF6::Pty Qt::Test
    )
endif()

ecm_add_test(
    BookMarkTest.cpp
    LINK_LIBRARIES KF6::Bookmarks Qt::Test
)

ecm_add_tests(
    CharacterColorTest.cpp
    CharacterTest.cpp
    CharacterWidthTest.cpp
    LINK_LIBRARIES konsolecharacters Qt::Test
)

ecm_add_tests(
    HistoryTest.cpp
    HotSpotFilterTest.cpp
    ScreenTest.cpp
    SessionTest.cpp
    ShellCommandTest.cpp
    TerminalTest.cpp
    LINK_LIBRARIES konsoleprivate Qt::Test
)

ecm_add_test(
    ProcessInfoTest.cpp
    LINK_LIBRARIES konsoleprivate KF6::CoreAddons Qt::Test
)

ecm_add_test(
    ProfileTest.cpp
    LINK_LIBRARIES konsoleprivate KF6::ConfigGui KF6::I18n Qt::Test
)

ecm_add_test(
    TerminalCharacterDecoderTest.cpp
    LINK_LIBRARIES konsoledecoders konsolecharacters konsolecolorscheme Qt::Test
)

ecm_add_test(
    Vt102EmulationTest.cpp
    LINK_LIBRARIES konsoleprivate Qt::Multimedia Qt::Test
)

ecm_add_test(
    ContainerDetectorParsingTest.cpp
    LINK_LIBRARIES konsolecontainers Qt::Test
)

if(NOT WIN32)
    ecm_add_test(
        PtyTest.cpp
        LINK_LIBRARIES konsoleprivate KF6::Pty Qt::Test
    )
endif()

ecm_add_test(
    TerminalInterfaceTest.cpp
    LINK_LIBRARIES konsoleprivate KF6::Parts Qt::Test
)
if (CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    target_link_libraries(TerminalInterfaceTest kvm)
endif()

ecm_add_test(
    ViewManagerTest.cpp
    LINK_LIBRARIES konsoleapp konsoleprivate Qt::Test
)
