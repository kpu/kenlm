#!/usr/bin/env bash
set -ex

# Install system packages required by libraries
yum install -y libunistring-devel

# Compile wheels
for PYBIN in /opt/python/cp38-cp38/bin /opt/python/cp39-cp39/bin /opt/python/cp310-cp310/bin; do
    "${PYBIN}/pip" wheel . -w /tmp/wheelhouse/ --no-deps
done

# Bundle external shared libraries into the wheels
for whl in /tmp/wheelhouse/*.whl; do
    auditwheel repair "$whl" -w wheelhouse/
done
