#!/bin/bash
#This is just an example compilation.  You should integrate these files into your build system.  Boost jam is provided and preferred.

echo Note: You must use ./bjam if you want language model estimation or filtering 1>&2

rm {lm,util}/*.o 2>/dev/null
set -e

CXXFLAGS="-I. -O3 -DNDEBUG -DKENLM_MAX_ORDER=6 $CXXFLAGS"

for i in util/{bit_packing,ersatz_progress,exception,file,file_piece,murmur_hash,mmap,pool,read_compressed,scoped,string_piece,usage} lm/{bhiksha,binary_format,config,lm_exception,model,quantize,read_arpa,search_hashed,search_trie,sizes,trie,trie_sort,value_build,virtual_interface,vocab} util/double-conversion/{bignum,bignum-dtoa,cached-powers,diy-fp,double-conversion,fast-dtoa,fixed-dtoa,strtod}; do
  g++ $CXXFLAGS -c $i.cc -o $i.o
done
mkdir -p bin
g++ $CXXFLAGS lm/build_binary_main.cc {lm,util,util/double-conversion}/*.o -o bin/build_binary
g++ $CXXFLAGS lm/query_main.cc {lm,util,util/double-conversion}/*.o -o bin/query
g++ $CXXFLAGS lm/kenlm_max_order_main.cc -o bin/kenlm_max_order
