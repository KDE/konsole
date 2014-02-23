include(ECMMarkAsTest)

set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})

set(KONSOLE_TEST_LIBS Qt5::Test konsoleprivate)

add_executable(PartManualTest PartManualTest.cpp)
ecm_mark_as_test(PartManualTest)
target_link_libraries(PartManualTest KF5::XmlGui KF5::Parts KF5::Pty
                     ${KONSOLE_TEST_LIBS})

