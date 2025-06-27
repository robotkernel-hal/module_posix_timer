import os
import shutil

from conan import ConanFile
from conan.tools.build import can_run
from conan.tools.files import copy

class TestTestConan(ConanFile):
    test_type = "explicit"
    settings = "os", "compiler", "build_type", "arch"
    generators = "VirtualRunEnv"

    def requirements(self):
        self.requires(self.tested_reference_str)

    def build(self):
        copy(self, "mod_test.rkc", self.recipe_folder, self.package_folder)

    def test(self):
        if can_run(self):
            self.run("pwd")
            self.run("robotkernel --test-run --config %s" % os.path.join(self.package_folder, "mod_test.rkc"), env="conanrun")
        else:
            self.output.warn("Skipping run cross built package")
