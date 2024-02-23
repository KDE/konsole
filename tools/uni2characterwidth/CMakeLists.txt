### uni2characterwidth
###
###   Converts Unicode Character Database files into character width lookup
###   tables. Uses a template file to place the tables in a source code file
###   together with a function for finding the width for specified character.
###
###   See `uni2characterwidth --help` for usage information
if(KONSOLE_BUILD_UNI2CHARACTERWIDTH)

# NOTE: this doesn't build with Qt6 even using compat and there are
#       some other errors that will need fixed.  For now if this
#       needs to be used, build it on a Qt5 system.

    find_package(Qt6 ${QT_MIN_VERSION} CONFIG REQUIRED
        Core
    )
    find_package(KF6 ${KF_MIN_VERSION} REQUIRED
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
        KF6::KIOCore
    )

endif()
