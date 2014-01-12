set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
include_directories(${KDE4_KPTY_INCLUDES}
                   ${KDE4_KPARTS_INCLUDES}
                   ${QT_QTTEST_INCLUDES})

set(KONSOLE_TEST_LIBS Qt5::Test konsoleprivate)

## If it's interactive, it shouldn't use kde4_add_unit_test,
##  since make test is supposed to be fully automated.

kde4_add_executable(PartManualTest TEST PartManualTest.cpp)
target_link_libraries(PartManualTest KF5::XmlGui KF5::Parts KF5::Pty
                               ${KONSOLE_TEST_LIBS})


