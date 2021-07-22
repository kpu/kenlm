# cnstrc_kenlm

This is a dedicated package to ship `kenlm` binary wheels along with binary executables
via _Internal PyPi server_.

## build

1. Install platform dependencies listed in [BUILDING](BUILDING).

2. Build wheels:

```shell
  ./build_cnstrc_wheel.sh
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