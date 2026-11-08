from conan import ConanFile
from conan.tools.cmake import cmake_layout


class Recipe(ConanFile):
    name = "evac"
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps", "VirtualRunEnv"

    def layout(self):
        self.folders.generators = "conan"

    def requirements(self):
        self.requires("fmt/11.1.3")
        self.requires("cpr/1.14.2")
        self.requires("nlohmann_json/3.11.3")
        self.requires("libarchive/3.7.9")
        self.requires("spdlog/1.15.1")
        self.requires("boost/1.91.0")
        self.requires("zoe/3.6")

    def build_requirements(self):
        self.test_requires("catch2/3.8.0")
        self.test_requires("cpp-httplib/0.62.0")

    def configure(self):
        # The ACL support of the libarchive package does not link on Linux,
        # and we do not need ACLs.
        self.options["libarchive"].with_acl = False
        # Requests run on worker threads, where curl must not use signals
        # (CURLOPT_NOSIGNAL).
        self.options["cpr"].signal = False
