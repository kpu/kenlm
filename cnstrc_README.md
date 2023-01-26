# cnstrc_kenlm

This is a dedicated package to ship `kenlm` binary wheels along with binary executables
via _Internal PyPi server_.

## build

1. Install platform dependencies listed in [BUILDING](BUILDING).

2. Build wheels for Linux:

```shell
  ./build_multiarch_linux_wheels.sh
```

3. Upload generated binary wheels to _Internal PyPi server_.

## usage

In addition to original `kenlm` package `cnstrc_kenlm` comes with a dedicated `kenlm_bin` module.
This is just a thin wrapper over external binary executables.

```python
import kenlm_bin

# example: call 'lmplz'
kenlm_bin.call(
    'lmplz', ['-o', '3', '--discount_fallback', '-S', '25%'],
    stdin=...,
    stdout=...,
    timeout=60,
)

# example: call 'build_binary'
kenlm_bin.call(
    'build_binary', ['-s', '<file_in>', '<file_out>'],
    timeout=60,
)
```

## manylinux wheels
To build cross platform **manylinux** wheels run the following from project root:
```console
docker build -t manylinux2014_for_kenlm .
docker run --rm -v ${PWD}:/io -w=/io manylinux2014_for_kenlm /bin/bash build_cnstrc_manylinux_wheel.sh
```

This will mount your working directory to Docker container and use it to build wheels and manylinux
wheels. The results of the build will be in `./wheels/manylinux`.

Upload generated binary wheels to _Internal PyPi server_.

## macOS wheels
Requirements:
 - be on macOS
 - have [delocate](https://github.com/matthew-brett/delocate) installed. You can install deloate by
 running 
```console"
pip install "delocate>=0.10.1"
```

To build **macOS** wheels with embedded libraries run the following from project root:
```console
./build_cnstrc_macos_wheel.sh
```

The results of the build will be in `./wheels/macos`.

Upload generated binary wheels to _Internal PyPi server_.

_Note: Currently we support both Intel and M1 chips, 
please ensure to have both binary wheels uploaded to _Internal PyPi server_._
