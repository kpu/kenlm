Dependencies
============

Misc Libraries
--------------

```bash
sudo apt-get install libboost1.48-dev
sudo apt-get install cmake
```

Getting TPIE
------------

This builder depends on TPIE (Templated Portable I/O Environment).
URL: https://github.com/thomasmoelhave/tpie

Instructions for obtaining, building and installing TPIE:

```bash
wget https://github.com/thomasmoelhave/tpie/archive/v1.0rc2.zip
unzip v1.0rc2.zip
cd tpie-1.0rc2/
cmake .
make -j4
sudo make install
```

Building
========

```bash
bjam
```

Testing
=======

```bash
# Create a test stream
bin/gcc-4.6/release/threading-multi/fs_test gen 3 test.bin

# Dump it to stdout
bin/gcc-4.6/release/threading-multi/fs_test dump 3 test.bin
```
