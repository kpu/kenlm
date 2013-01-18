#!/bin/bash
#This is just an example compilation.  You should integrate these files into your build system.  Boost jam is provided and preferred.  
#If your code uses ICU, define HAVE_ICU
#I use zlib by default.  If you don't want to depend on zlib, remove -DHAVE_ZLIB and -lz.  

echo Note: You must use ./bjam if you want language model estimation or filtering 1>&2

rm {lm,util}/*.o 2>/dev/null
set -e

CXXFLAGS="-I. -O3 -DNDEBUG -DHAVE_ZLIB -DKENLM_MAX_ORDER=6 $CXXFLAGS"

for i in util/{bit_packing,ersatz_progress,exception,file,file_piece,murmur_hash,mmap,pool,read_compressed,scoped,string_piece,usage} lm/{bhiksha,binary_format,config,lm_exception,model,quantize,read_arpa,search_hashed,search_trie,sizes,trie,trie_sort,value_build,virtual_interface,vocab} util/double-conversion/{bignum,bignum-dtoa,cached-powers,diy-fp,double-conversion,fast-dtoa,fixed-dtoa,strtod}; do
  g++ $CXXFLAGS -c $i.cc -o $i.o
done
g++ $CXXFLAGS lm/build_binary.cc {lm,util,util/double-conversion}/*.o -lz -o build_binary
g++ $CXXFLAGS lm/ngram_query.cc {lm,util,util/double-conversion}/*.o -lz -o query
g++ $CXXFLAGS lm/max_order.cc -o kenlm_max_order
