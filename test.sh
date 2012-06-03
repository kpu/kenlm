#!/bin/bash
#Run tests.  Requires Boost.
set -e
./compile.sh
for i in util/{bit_packing,file_piece,joint_sort,probing_hash_table,sorted_uniform}_test lm/{model,left}_test; do
  g++ -I. -O3 $CXXFLAGS $i.cc {lm,util}/*.o -DBOOST_TEST_DYN_LINK -lboost_unit_test_framework -lz -o $i
  pushd $(dirname $i) >/dev/null && ./$(basename $i) || echo "$i failed"; popd >/dev/null
done 
