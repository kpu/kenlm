
PIP_NO_INDEX=False # Allow requisites from PyPi
PIP_NO_DEPENDENCIES=False # Install dependencies from our defined dependencies
PIP_IGNORE_INSTALLED=False # Take into account the current installed dependencies

BDIR="./build"

[[ -d $BDIR ]] && rm -rf $BDIR
mkdir $BDIR

# Build
pushd $BDIR
cmake .. -DKENLM_MAX_ORDER=$KENLM_MAX_ORDER -DCMAKE_INSTALL_PREFIX:PATH=$PREFIX
# Install
make -j all install
popd
rm -rf $BDIR

# Install python module
$PYTHON -m pip install . --install-option="--max_order $KENLM_MAX_ORDER"
