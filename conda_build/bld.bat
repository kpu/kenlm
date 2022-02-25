
@echo off

set BDIR="./build"

if exist %BDIR% ( rmdir /S /Q %BDIR% )
mkdir %BDIR%

echo " - Building..."
pushd %BDIR%
cmake .. -DKENLM_MAX_ORDER=%KENLM_MAX_ORDER% -DCMAKE_INSTALL_PREFIX:PATH=%PREFIX%
echo " - Installing..."
make -j all install
popd
rmdir /S /Q %BDIR%

echo " - Installing python module..."
"%PYTHON%" -m pip install . --install-option="--max_order %KENLM_MAX_ORDER%"
