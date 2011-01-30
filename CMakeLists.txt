find_package(KDE4 4.6.40 REQUIRED)

KDE4_NO_ENABLE_FINAL(konsole)

add_subdirectory( src ) 
add_subdirectory( data ) 
add_subdirectory( desktop )


