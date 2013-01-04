#!/bin/bash
#Run tests.  Requires Boost.
. compile.sh
for i in util/{bit_packing,file_piece,joint_sort,probing_hash_table,read_compressed,sorted_uniform}_test lm/{model,left}_test; do
  g++ $CXXFLAGS $i.cc {lm,util}/*.o -DBOOST_TEST_DYN_LINK -lboost_unit_test_framework -lz -o $i
  pushd $(dirname $i) >/dev/null && ./$(basename $i) || echo "$i failed"; popd >/dev/null
done 
