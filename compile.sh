#!/bin/bash
#This is just an example compilation.  You should integrate these files into your build system.  I can provide boost jam if you want.  
#If your code uses ICU, edit util/string_piece.hh and uncomment #define USE_ICU
#I use zlib by default.  If you don't want to depend on zlib, remove #define USE_ZLIB from util/file_piece.hh

#don't need to use if compiling with moses Makefiles already

rm {lm,util}/*.o 2>/dev/null
set -e

for i in util/{bit_packing,ersatz_progress,exception,file_piece,murmur_hash,file,mmap,usage} lm/{bhiksha,binary_format,config,lm_exception,model,quantize,read_arpa,search_hashed,search_trie,trie,trie_sort,value_build,virtual_interface,vocab}; do
  g++ -I. -O3 -DNDEBUG $CXXFLAGS -c $i.cc -o $i.o
done
g++ -I. -O3 -DNDEBUG $CXXFLAGS lm/build_binary.cc {lm,util}/*.o -lz -o build_binary
g++ -I. -O3 -DNDEBUG $CXXFLAGS lm/ngram_query.cc {lm,util}/*.o -lz -o query
g++ -I. -O3 -DNDEBUG $CXXFLAGS lm/max_order.cc -o kenlm_max_order
