add_subdirectory(demo_konsolepart)

include(ECMMarkAsTest)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt::Test konsoleprivate)

add_executable(PartManualTest PartManualTest.cpp)
ecm_mark_as_test(PartManualTest)
target_link_libraries(PartManualTest KF${QT_MAJOR_VERSION}::XmlGui KF${QT_MAJOR_VERSION}::Parts KF${QT_MAJOR_VERSION}::Pty
                     ${KONSOLE_TEST_LIBS})

