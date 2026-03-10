from conan import ConanFile, conan_version
from conan.tools.scm import Version
import os


class MainProject(ConanFile):
    python_requires = "conan_template/[~5]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "module_posix_timer"
    description = "module_posix_timer is used to generate deterministic triggers for other modules."
    exports_sources = ["*", "!.gitignore"]
    tool_requires = [
        "robotkernel_generator/[~6]@robotkernel/unstable" ]

    requires = [
        "robotkernel/[~6]@robotkernel/unstable",
        "service_provider_process_data_inspection/[~6]@robotkernel/unstable" ]

    def source(self):
        self.run(f"sed 's/AC_INIT(.*/AC_INIT([{self.name}], [{self.version}], [{self.author}])/' configure.ac.in > configure.ac")

