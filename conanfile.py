from conan import ConanFile
import os


class MainProject(ConanFile):
    python_requires = "conan_template/[~=5]@robotkernel/stable"
    python_requires_extend = "conan_template.RobotkernelConanFile"

    name = "module_posix_timer"
    description = "module_posix_timer is used to generate deterministic triggers for other modules."
    exports_sources = ["*", "!.gitignore"]
    requires = ["robotkernel/[~=5]@robotkernel/stable", "service_provider_process_data_inspection/[~=5]@robotkernel/stable"]
