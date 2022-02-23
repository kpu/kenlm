
DIR="${RECIPE_DIR}/${RDIR}"

echo "DIR: $DIR"
echo "KENLM_MAX_ORDER: $KENLM_MAX_ORDER"

[[ -d $DIR/build ]] && rm -rf $DIR/build
mkdir -p $DIR/build

echo -e " - \e[4mBuilding...\e[0m"
cd $DIR/build
cmake .. -DKENLM_MAX_ORDER=$KENLM_MAX_ORDER -DCMAKE_INSTALL_PREFIX:PATH=$PREFIX
echo -e " - \e[4mInstalling...\e[0m"
make -j all install
cd $DIR
rm -rf $DIR/build

echo -e " - \e[4mInstalling python module...\e[0m"
$PYTHON -m pip install $DIR --install-option="--max_order $KENLM_MAX_ORDER"
