from distutils.core import setup
from distutils.extension import Extension
import glob
import platform

FILES = glob.glob('util/*.cc') + glob.glob('lm/*.cc') + glob.glob('util/double-conversion/*.cc')
FILES = [fn for fn in FILES if not (fn.endswith('main.cc') or fn.endswith('test.cc'))]

LIBS = ['z', 'stdc++']
if platform.system() != 'Darwin':
    LIBS.append('rt')

ext_modules = [
    Extension(name='kenlm',
        sources=FILES + ['python/kenlm.cpp'],
        language='C++', 
        include_dirs=['.'],
        libraries=LIBS, 
        extra_compile_args=['-O3', '-DNDEBUG', '-DKENLM_MAX_ORDER=6'])
]

setup(
    name='kenlm',
    ext_modules=ext_modules
)
