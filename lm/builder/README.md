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

```bash
$ bin/lmplz
```

```
Language model building options:
  -o [ --order ] arg                  Order of the model
  -t [ --temp_prefix ] arg (=/tmp/lm) Temporary file prefix
  -v [ --vocab_file ] arg             Location to write vocabulary file
  -b [ --block_size ] arg (=67108864) Block size
  --block_count arg (=2)              Block count (per order)
  -a [ --sort_arity ] arg (=4)        Arity to use for sorting
  --sort_buffer arg (=67108864)       Sort read buffer size
  --sort_lazy_arity arg (=2)          Lazy sorting arity (this * order readers 
                                      active)
  --sort_lazy_buffer arg (=33554432)  Lazy sorting read buffer size
  --interpolate_unigrams              Interpolate the unigrams (default: 
                                      emulate SRILM by not interpolating)
  -V [ --verbose_header ]             Add a verbose header to the ARPA file 
                                      that includes information such as token 
                                      count, smoothing type, etc.
```


Running
=======

```bash
bin/lmplz -o 5 <text >text.arpa
```
