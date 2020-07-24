ki18n_wrap_ui(konsoleprofile_SRCS
    EditProfileGeneralPage.ui
    EditProfileTabsPage.ui
    EditProfileAppearancePage.ui
    EditProfileScrollingPage.ui
    EditProfileKeyboardPage.ui
    EditProfileMousePage.ui
    EditProfileAdvancedPage.ui
)

add_library(konsoleprofile
STATIC
    EditProfileDialog.cpp
    Profile.cpp
    ProfileList.cpp
    ProfileReader.cpp
    ProfileWriter.cpp
    ProfileManager.cpp
)

target_link_libraries(
    konsoleprofile
    ${konsole_LIBS}
    konsoleprivate_core
)
