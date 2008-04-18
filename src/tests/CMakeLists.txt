set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${KDE4_KPTY_INCLUDES} ${KDE4_KPARTS_INCLUDES} ${QT_QTTEST_INCLUDES})

kde4_add_unit_test(PartTest PartTest.cpp)
target_link_libraries(PartTest ${KDE4_KPARTS_LIBS} ${KDE4_KPTY_LIBS} ${QT_QTTEST_LIBRARY}
						kdeinit_konsole)
