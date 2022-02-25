
BDIR="./build"

[[ -d $BDIR ]] && rm -rf $BDIR
mkdir $BDIR

echo " - Building..."
pushd $BDIR
cmake .. -DKENLM_MAX_ORDER=$KENLM_MAX_ORDER -DCMAKE_INSTALL_PREFIX:PATH=$PREFIX
echo " - Installing..."
make -j all install
popd
rm -rf $BDIR

echo " - Installing python module..."
$PYTHON -m pip install . --install-option="--max_order $KENLM_MAX_ORDER"
