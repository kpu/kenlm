#!/usr/bin/env bash
set -euxo pipefail

rm -rf wheelhouse || true

echo "PWD ${PWD}"

# Cross-platform way to create a temporary dir.
mytmpdir=$(mktemp -d 2>/dev/null || mktemp -d -t 'mytmpdir')

docker run --rm -v ${PWD}:/io \
    -w=/io \
    quay.io/pypa/manylinux2014_aarch64 \
    /bin/bash build_manylinux1_wheels.sh
mv wheelhouse "${mytmpdir}/wheelhouse_aarch64"

docker run --rm -v ${PWD}:/io \
    -w=/io \
    quay.io/pypa/manylinux2014_x86_64 \
    /bin/bash build_manylinux1_wheels.sh
mv wheelhouse "${mytmpdir}/wheelhouse_x86_64"


mkdir wheelhouse
mv ${mytmpdir}/wheelhouse_x86_64/* wheelhouse
mv ${mytmpdir}/wheelhouse_aarch64/* wheelhouse
rm -rf "${mytmpdir}/wheelhouse_x86_64" "${mytmpdir}/wheelhouse_aarch64"
