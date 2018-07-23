from conans import ConanFile, CMake


class KonsoleConan(ConanFile):
    name = "konsole"
    version = "18.11.70"
    license = "GPLv2"
    url = "https://www.kde.org/applications/system/konsole/"
    description = "Konsole is the terminal emulator"

    settings = "os", "compiler", "build_type", "arch"

    build_requires = (
        "extra-cmake-modules/1.6.0@kde/testing",

        "Qt/5.6.3@bincrafters/stable"
        # "qt-core/5.6.0@qt/testing",
        # "qt-dbus/5.6.0@qt/testing",
        # "qt-printingsupport/5.6.0@qt/testing",
        # "qt-widgets/5.6.0@qt/testing",

        "kf5-Bookmarks/5.6.0@kde/testing",
        "kf5-Completion/5.6.0@kde/testing",
        "kf5-Config/5.6.0@kde/testing",
        "kf5-ConfigWidgets/5.6.0@kde/testing",

        "kf5-CoreAddons/5.6.0@kde/testing",
        "kf5-Crash/5.6.0@kde/testing",
        "kf5-GuiAddons/5.6.0@kde/testing",
        "kf5-DBusAddons/5.6.0@kde/testing",

        "kf5-I18n/5.6.0@kde/testing",
        "kf5-IconThemes/5.6.0@kde/testing",
        "kf5-Init/5.6.0@kde/testing",
        "kf5-KIO/5.6.0@kde/testing",
        "kf5-NewStuff/5.6.0@kde/testing",
        "kf5-NewStuffCore/5.6.0@kde/testing",
        "kf5-Notifications/5.6.0@kde/testing",
        "kf5-NotifyConfig/5.6.0@kde/testing",

        "kf5-Parts/5.6.0@kde/testing",
        "kf5-Pty/5.6.0@kde/testing",
        "kf5-Service/5.6.0@kde/testing",
        "kf5-TextWidgets/5.6.0@kde/testing",
        "kf5-WidgetsAddons/5.6.0@kde/testing",

        "kf5-WindowSystem/5.6.0@kde/testing",
        "kf5-XmlGui/5.6.0@kde/testing",
        "kf5-DBusAddons/5.6.0@kde/testing",
        "kf5-GlobalAccel/5.6.0@kde/testing",

        "kf5-DocTools/5.6.0@kde/testing",
    )

    generators = "cmake"

    def requirements(self):
        if self.settings.os != "Macos":
            self.requires("x11/latest@kde/testing")

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        self.copy("*.h", dst="include", src="src")
        self.copy("*.lib", dst="lib", keep_path=False)
        self.copy("*.dll", dst="bin", keep_path=False)
        self.copy("*.dylib*", dst="lib", keep_path=False)
        self.copy("*.so", dst="lib", keep_path=False)
        self.copy("*.a", dst="lib", keep_path=False)
