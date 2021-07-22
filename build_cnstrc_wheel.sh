#!/bin/bash

set -e

# re-create build dir
rm -rf build && mkdir build

# build binaries in build dir
cd build && cmake .. && make -j 8 && cd ..

# copy binaries into kenlm_bin dir
cp -r build/bin kenlm_bin/

# build the cnstrc_kenlm wheel
pip wheel .
