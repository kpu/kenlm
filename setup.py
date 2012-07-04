from distutils.core import setup
from distutils.extension import Extension

UTIL = ('bit_packing', 'ersatz_progress', 'exception', 'file_piece', 
        'murmur_hash', 'file', 'mmap', 'usage')
LM = ('bhiksha', 'binary_format', 'config', 'lm_exception', 'model',
      'quantize', 'read_arpa', 'search_hashed', 'search_trie', 'trie',
      'trie_sort', 'value_build', 'virtual_interface', 'vocab')

FILES = ['util/'+fn+'.cc' for fn in UTIL] + ['lm/'+fn+'.cc' for fn in LM]

ext_modules = [
    Extension(name='kenlm',
        sources=FILES + ['python/kenlm.cpp'],
        language='C++', 
        include_dirs=['.'],
        libraries=['z', 'stdc++'], 
        extra_compile_args=['-O3', '-DNDEBUG'])
]

setup(
    name='kenlm',
    ext_modules=ext_modules
)
