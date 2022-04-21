### uni2characterwidth
###
###   Converts Unicode Character Database files into character width lookup
###   tables. Uses a template file to place the tables in a source code file
###   together with a function for finding the width for specified character.
###
###   See `uni2characterwidth --help` for usage information
if(KONSOLE_BUILD_UNI2CHARACTERWIDTH)

    find_package(Qt${QT_MAJOR_VERSION} ${QT_MIN_VERSION} CONFIG REQUIRED
        Core
    )
    find_package(KF5 ${KF5_MIN_VERSION} REQUIRED
        KIO
    )

    set(uni2characterwidth_SRC
        uni2characterwidth.cpp
        properties.h
        template.cpp
        template.h
    )

    add_executable(uni2characterwidth ${uni2characterwidth_SRC})
    target_link_libraries(uni2characterwidth
        Qt::Core
        KF5::KIOCore
    )

endif()
