#!/bin/bash
#This is just an example compilation.  You should integrate these files into your build system.  Boost jam is provided and preferred.  
#If your code uses ICU, define HAVE_ICU
#I use zlib by default.  If you don't want to depend on zlib, remove -DHAVE_ZLIB and -lz.  

rm {lm,util}/*.o 2>/dev/null
set -e

for i in util/{bit_packing,ersatz_progress,exception,file,file_piece,murmur_hash,mmap,read_compressed,string_piece,usage} lm/{bhiksha,binary_format,config,lm_exception,model,quantize,read_arpa,search_hashed,search_trie,trie,trie_sort,value_build,virtual_interface,vocab}; do
  g++ -I. -O3 -DNDEBUG -DHAVE_ZLIB $CXXFLAGS -c $i.cc -o $i.o
done
g++ -I. -O3 -DNDEBUG $CXXFLAGS lm/build_binary.cc {lm,util}/*.o -lz -o build_binary
g++ -I. -O3 -DNDEBUG $CXXFLAGS lm/ngram_query.cc {lm,util}/*.o -lz -o query
g++ -I. -O3 -DNDEBUG $CXXFLAGS lm/max_order.cc -o kenlm_max_order
