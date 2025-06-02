import os
import shutil

from conan import ConanFile
from conan.tools.build import can_run


class TestTestConan(ConanFile):
    test_type = "explicit"
    settings = "os", "compiler", "build_type", "arch"
    exports_sources = "mod_test.rkc"
    generators = "VirtualRunEnv"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def test(self):
        if can_run(self):
            self.run("robotkernel --test-run --config .%smod_test.rkc" % os.sep, env="conanrun")
        else:
            self.output.warn("Skipping run cross built package")
