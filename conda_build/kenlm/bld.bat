
@echo off

set PIP_NO_INDEX="False"
set PIP_NO_DEPENDENCIES="False"
set PIP_IGNORE_INSTALLED="False"

set BDIR="./build"

if exist %BDIR% ( rmdir /S /Q %BDIR% )
mkdir %BDIR%

:: Build
pushd %BDIR%
cmake .. -DKENLM_MAX_ORDER=%KENLM_MAX_ORDER% -DCMAKE_INSTALL_PREFIX:PATH=%PREFIX%
:: Install
make -j all install
popd
rmdir /S /Q %BDIR%

:: Install python module
"%PYTHON%" -m pip install . --install-option="--max_order %KENLM_MAX_ORDER%"
