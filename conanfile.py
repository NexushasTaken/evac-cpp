from conan import ConanFile
from conan.tools.cmake import cmake_layout


class EvacConan(ConanFile):
    name = "evac"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    # gtkmm-4.0 is not on Conan Center, so it comes from the system
    # (found with pkg-config in CMakeLists.txt).
    def requirements(self):
        self.requires("libcurl/8.12.1")
        self.requires("nlohmann_json/3.11.3")
        self.requires("libarchive/3.7.9")
        self.requires("spdlog/1.15.1")
        self.test_requires("catch2/3.8.0")

    def configure(self):
        # The ACL support of the libarchive package does not link on Linux,
        # and we do not need ACLs.
        self.options["libarchive"].with_acl = False

    def layout(self):
        cmake_layout(self)
