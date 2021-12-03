#!/bin/bash

set -e

./build_cnstrc_wheel.sh

# repair wheels to be manylinux
auditwheel repair wheels/tmp/cnstrc_kenlm*linux_x86_64.whl -w wheels/manylinux
