from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext as _build_ext
import platform
import subprocess
import os
import sys
import re
from pathlib import Path

VERSION = "0.2.0"

# Use an environment variable
max_order = os.getenv("MAX_ORDER", "6")

# Try to get from --config-settings, if present
is_max_order = [s for s in sys.argv if "--max_order" in s]
for element in is_max_order:
    max_order = re.split("[= ]", element)[1]
    sys.argv.remove(element)

print(f"Will build with KenLM max_order set to {max_order}")


class build_ext(_build_ext):
    def run(self):
        try:
            out = subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError(
                "CMake must be installed to build the following extensions: "
                + ", ".join(e.name for e in self.extensions)
            )

        ext_dir = str(Path(self.get_ext_fullpath("kenlm")).absolute().parent)
        source_dir = str(Path(__file__).absolute().parent)

        cmake_args = [
            f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY={ext_dir}",
            "-DBUILD_SHARED_LIBS=ON",
            "-DBUILD_PYTHON_STANDALONE=ON",
            f"-DKENLM_MAX_ORDER={max_order}",
            f"-DCMAKE_PROJECT_VERSION={VERSION}",
        ]
        cfg = "Debug" if self.debug else "Release"
        build_args = ["--config", cfg]

        if platform.system() == "Windows":
            cmake_args += [
                "-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON",
                f"-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_{cfg.upper()}={ext_dir}",
                f"-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{cfg.upper()}={ext_dir}",
                f"-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_{cfg.upper()}={ext_dir}",
            ]
            if sys.maxsize > 2**32:
                cmake_args += ["-A", "x64"]
            # build_args += ["--", "/m"]
        else:
            cmake_args.append(f"-DCMAKE_BUILD_TYPE={cfg}")

        env = os.environ.copy()
        env["CXXFLAGS"] = '{} -fPIC -DVERSION_INFO=\\"{}\\"'.format(
            env.get("CXXFLAGS", ""), self.distribution.get_version()
        )

        if not os.path.exists(self.build_temp):
            os.makedirs(self.build_temp)
        subprocess.check_call(
            ["cmake", source_dir] + cmake_args, cwd=self.build_temp, env=env
        )
        subprocess.check_call(
            ["cmake", "--build", ".", "-j", "4"] + build_args, cwd=self.build_temp
        )


ext_modules = [
    Extension(
        name="kenlm",
        language="C++",
        sources=[],
        depends=["python/BuildStandalone.cmake"],
    ),
]

setup(
    name="kenlm",
    version=VERSION,
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    include_package_data=True,
)
