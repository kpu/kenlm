
@echo off

set DIR="%RECIPE_DIR%/%RDIR%"

echo "DIR: %DIR%"
echo "KENLM_MAX_ORDER: %KENLM_MAX_ORDER%"

if exist %DIR%/build ( rmdir /S /Q %DIR%/build )
mkdir %DIR%/build

echo " - Building..."
cd %DIR%/build
cmake .. -DKENLM_MAX_ORDER=%KENLM_MAX_ORDER% -DCMAKE_INSTALL_PREFIX:PATH=%PREFIX%
echo " - Installing..."
make -j all install
cd %DIR%
rmdir /S /Q %DIR%/build

echo " - Installing python module..."
"%PYTHON%" -m pip install "%DIR%" --install-option="--max_order %KENLM_MAX_ORDER%"
