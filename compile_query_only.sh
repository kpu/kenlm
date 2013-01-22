#!/bin/bash
#This is just an example compilation.  You should integrate these files into your build system.  Boost jam is provided and preferred.

echo Note: You must use ./bjam if you want language model estimation or filtering 1>&2

rm {lm,util}/*.o 2>/dev/null
set -e

CXXFLAGS="-I. -O3 -DNDEBUG -DKENLM_MAX_ORDER=6 $CXXFLAGS"

#Grab all cc files in these directories except those ending in test.cc or main.cc
objects=""
for i in util/double-conversion/*.cc util/*.cc lm/*.cc; do
  if [ "${i%test.cc}" == "$i" ] && [ "${i%main.cc}" == "$i" ]; then
    g++ $CXXFLAGS -c $i -o ${i%.cc}.o
    objects="$objects ${i%.cc}.o"
  fi
done

mkdir -p bin
g++ $CXXFLAGS lm/build_binary_main.cc $objects -o bin/build_binary
g++ $CXXFLAGS lm/query_main.cc $objects -o bin/query
g++ $CXXFLAGS lm/kenlm_max_order_main.cc -o bin/kenlm_max_order
