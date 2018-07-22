from conans import ConanFile, CMake


class KonsoleConan(ConanFile):
    name = "konsole"
    version = "18.11.70"
    license = "GPLv2"
    url = "https://www.kde.org/applications/system/konsole/"
    description = "Konsole is the terminal emulator"

    settings = "os", "compiler", "build_type", "arch"

    generators = "cmake"

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
