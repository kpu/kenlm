
@echo off

set DIR="%RECIPE_DIR%/%RDIR%"

echo "DIR: %DIR%"
echo "KENLM_MAX_ORDER: %KENLM_MAX_ORDER%"

mkdir %DIR%/build_conda

echo " - Building..."
cd %DIR%/build_conda
cmake .. -DKENLM_MAX_ORDER=%KENLM_MAX_ORDER% -DCMAKE_INSTALL_PREFIX:PATH=%PREFIX%
echo " - Installing..."
make -j all install
cd %DIR%
rmdir /S /Q %DIR%/build_conda

echo " - Installing python module..."
"%PYTHON%" -m pip install "%DIR%" --install-option="--max_order %KENLM_MAX_ORDER%"
