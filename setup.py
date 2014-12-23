from distutils.core import setup
from distutils.extension import Extension
import glob
import platform
import os

#Does gcc compile with this header and library?
def compile_test(header, library):
    dummy_path = os.path.join(os.path.realpath(__file__), "dummy")
    return os.system("bash -c \"g++ -include " + header + " -l" + library + " -x c++ - <<<'int main() {}' -o " + dummy_path + ">/dev/null 2>/dev/null && rm " + dummy_path + " 2>/dev/null\"") == 0


FILES = glob.glob('util/*.cc') + glob.glob('lm/*.cc') + glob.glob('util/double-conversion/*.cc')
FILES = [fn for fn in FILES if not (fn.endswith('main.cc') or fn.endswith('test.cc'))]

LIBS = ['stdc++']
if platform.system() != 'Darwin':
    LIBS.append('rt')


ARGS = ['-O3', '-DNDEBUG', '-DKENLM_MAX_ORDER=6']

if compile_test("zlib.h", "z"):
    ARGS += "-DHAVE_ZLIB"
    LIBS += "z"

if compile_test("bzlib.h", "bz2"):
    ARGS += "-DHAVE_BZLIB"
    LIBS += "bz2"

if compile_test("lzma.h", "lzma"):
    ARGS += "-DHAVE_XZLIB"
    LIBS += "lzma"

ext_modules = [
    Extension(name='kenlm',
        sources=FILES + ['python/kenlm.cpp'],
        language='C++', 
        include_dirs=['.'],
        libraries=LIBS, 
        extra_compile_args=ARGS)
]

setup(
    name='kenlm',
    ext_modules=ext_modules
)
