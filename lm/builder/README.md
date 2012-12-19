Dependencies
============

lmplz requires boost 1.48+, which is available in the Ubuntu repositories as of 12.04+:

```bash
sudo apt-get install libboost1.48-all-dev
```

Alternatively, you can download, compile, and install it yourself:

```bash
wget http://sourceforge.net/projects/boost/files/boost/1.52.0/boost_1_52_0.tar.gz/download -O boost_1_52_0.tar.gz
tar -xvzf boost_1_52_0.tar.gz
cd boost_1_52_0
./bootstrap.sh
./b2
sudo ./b2 install
```

Local install options (in a user-space prefix directory) are also possible. See http://www.boost.org/doc/libs/1_52_0/doc/html/bbv2/installation.html.


Building
========

```bash
bjam
```

Usage
=====

```
$ bin/lmplz
```

Running
=======

```bash
bin/lmplz -o 5 <text >text.arpa
```
