import os
from conan import ConanFile, conan_version
from conan.tools.files import copy, chdir
from conan.tools.scm import Version


class MainProject(ConanFile):
    name = "module_posix_timer_ln_msgdef"
    url = "https://rmc-github.robotic.dlr.de/robotkernel/module_posix_timer"
    author = "Robert Burger robert.burger@dlr.de"
    license = "GPLv3"
    description = "module_posix_timer ln message definitions"
    settings = None
    exports_sources = [
        "*",
    ]

    tool_requires = ["robotkernel_ln_helper/[~6]@robotkernel/stable"]
    generators = "VirtualBuildEnv"
    package_type = "application"

    def package(self):
        ln_msg_dir = "share/ln/message_definitions"

        svc_def_files = []
        with chdir(self, self.build_folder):
            for dirpath, dirnames, filenames in os.walk("module_posix_timer"):
                svc_def_files.extend(os.path.join(dirpath, filename) for filename in filenames)

        self.run(
            "service_generate --indir . --outdir %s %s" % (ln_msg_dir, " ".join(svc_def_files)), env="conanbuild"
        )

        copy(self, "share/ln/message_definitions/*", self.build_folder, self.package_folder)

    def package_info(self):
        msgdef_dir = os.path.join(self.package_folder, "share/ln/message_definitions")
        if Version(conan_version) < "2.0.0":
            self.env_info.LN_MESSAGE_DEFINITION_DIRS.append(msgdef_dir)
        self.runenv_info.append_path("LN_MESSAGE_DEFINITION_DIRS", msgdef_dir)
        self.buildenv_info.append_path("LN_MESSAGE_DEFINITION_DIRS", msgdef_dir)
