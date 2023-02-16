from setuptools import setup, Extension
from setuptools.command.build_ext import build_ext as _build_ext
import glob
import platform
import subprocess
import os
import sys
import re
from pathlib import Path

#Does gcc compile with this header and library?
def compile_test(header, library):
    dummy_path = os.path.join(os.path.dirname(__file__), "dummy")
    command = "bash -c \"g++ -include " + header + " -l" + library + " -x c++ - <<<'int main() {}' -o " + dummy_path + " >/dev/null 2>/dev/null && rm " + dummy_path + " 2>/dev/null\""
    return os.system(command) == 0

max_order = "6"
is_max_order = [s for s in sys.argv if "--max_order" in s]
for element in is_max_order:
    max_order = re.split('[= ]',element)[1]
    sys.argv.remove(element)

FILES = glob.glob('util/*.cc') + glob.glob('lm/*.cc') + glob.glob('util/double-conversion/*.cc') + glob.glob('python/*.cc')
FILES = [fn for fn in FILES if not (fn.endswith('main.cc') or fn.endswith('test.cc'))]

#We don't need -std=c++11 but python seems to be compiled with it now.  https://github.com/kpu/kenlm/issues/86
ARGS = ['-O3', '-DNDEBUG', '-DKENLM_MAX_ORDER='+max_order, '-std=c++11']
INCLUDE_PATHS = []

if platform.system() == 'Linux':
    LIBS = ['stdc++', 'rt']
    ARGS.append('-DHAVE_CLOCKGETTIME')
elif platform.system() == 'Darwin':
    LIBS = ['c++']
else:
    LIBS = []

#Attempted fix to https://github.com/kpu/kenlm/issues/186 and https://github.com/kpu/kenlm/issues/197
if platform.system() == 'Darwin':
    ARGS += ["-stdlib=libc++", "-mmacosx-version-min=10.7"]
    INCLUDE_PATHS.append("/usr/local/include")

if compile_test('zlib.h', 'z'):
    ARGS.append('-DHAVE_ZLIB')
    LIBS.append('z')

if compile_test('bzlib.h', 'bz2'):
    ARGS.append('-DHAVE_BZLIB')
    LIBS.append('bz2')

if compile_test('lzma.h', 'lzma'):
    ARGS.append('-DHAVE_XZLIB')
    LIBS.append('lzma')


class build_ext(_build_ext):
    def run(self):
        try:
            out = subprocess.check_output(["cmake", "--version"])
        except OSError:
            raise RuntimeError(
                "CMake must be installed to build the following extensions: "
                + ", ".join(e.name for e in self.extensions)
            )

        ext_dir = str(Path(self.get_ext_fullpath('libkenlm')).absolute().parent)
        source_dir = str(Path(__file__).absolute().parent)

        cmake_args = [
            "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY=" + ext_dir,
            "-DBUILD_SHARED_LIBS=ON",
            "-DBUILD_PYTHON_STANDALONE=ON",
        ]
        cfg = "Debug" if self.debug else "Release"
        build_args = ["--config", cfg]

        if platform.system() == "Windows":
            cmake_args += [
                "-DCMAKE_WINDOWS_EXPORT_ALL_SYMBOLS=ON",
                "-DCMAKE_RUNTIME_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), ext_dir),
                "-DCMAKE_LIBRARY_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), ext_dir),
                "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY_{}={}".format(cfg.upper(), ext_dir),
            ]
            if sys.maxsize > 2**32:
                cmake_args += ["-A", "x64"]
            build_args += ["--", "/m"]
        else:
            cmake_args += ["-DCMAKE_BUILD_TYPE=" + cfg]
            build_args += ["--", "-j4"]

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
            ["cmake", "--build", "."] + build_args, cwd=self.build_temp
        )

        return _build_ext.run(self)


ext_modules = [
    Extension(name='kenlm',
        sources=FILES + ['python/kenlm.cpp'],
        language='C++', 
        include_dirs=['.'] + INCLUDE_PATHS,
        libraries=LIBS, 
        extra_compile_args=ARGS),
]

setup(
    name='kenlm',
    ext_modules=ext_modules,
    cmdclass={"build_ext": build_ext},
    include_package_data=True,
)
