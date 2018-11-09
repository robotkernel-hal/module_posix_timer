from conans import ConanFile, AutoToolsBuildEnvironment


class MainProject(ConanFile):
    name = "module_posix_timer"
    license = "GPLv3"
    url = "https://rmc-github.robotic.dlr.de/robotkernel/module_posix_timer"
    description = "module_posix_timer is used to generate deterministic triggers for other modules."
    settings = "os", "compiler", "build_type", "arch"
    scm = {
        "type": "git",
        "url": "auto",
        "revision": "auto",
        "submodule": "recursive",
    }

    generators = "pkg_config"
    requires = "robotkernel/[~=5.0]@common/unstable"

    def build(self):
        self.run("autoreconf -if")
        autotools = AutoToolsBuildEnvironment(self)
        autotools.configure(configure_dir=".", host=self.settings.arch )
        autotools.make()

    def package(self):
        autotools = AutoToolsBuildEnvironment(self)
        autotools.install()

    def package_info(self):
        self.cpp_info.includedirs = ['include']
        self.cpp_info.bindirs = ['bin']
        self.cpp_info.resdirs = ['share']
