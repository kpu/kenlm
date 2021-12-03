#!/bin/bash

set -e

./build_cnstrc_wheel.sh

# repair wheels to have embedded libraries
delocate-wheel wheels/tmp/cnstrc_kenlm*macosx*.whl -w wheels/macos
