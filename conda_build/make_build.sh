#!/usr/bin/env bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PACKAGE_VERBOSE_NAME="KenLM"

usage()
{
  echo "$(basename $0) [-h] [-e <conda_env_name>] [-r] [-s] -n <package_name> -p <python_version>"
  echo ""
  echo "OPTIONS:"
  echo "  -h                      It displays this help message."
  echo "  -e <conda_env_name>     Name of the conda environment which will be used to build"
  echo "                          ${PACKAGE_VERBOSE_NAME}. If not specified, the default value is"
  echo "                          the provided package name with the suffix '-build'."
  echo "  -r                      It removes the conda environment before start if exists."
  echo "  -s                      Skip untracked files check."
  echo "  -n <package_name>       Conda package name in order to retrieve the pkg path"
  echo "  -p <python_version>     Python version to be used to build ${PACKAGE_VERBOSE_NAME}."
}

CONDA_ENV_NAME=""
CONDA_PACKAGE_NAME=""
REMOVE_ENV_IF_EXISTS=""
CONDA_PYTHON_VERSION=""
SKIP_UNTRACKED_FILES_CHECK=""

while getopts "e:n:p:rsh" options
do
  case "${options}" in
    h) usage
       exit 0;;
    e) CONDA_ENV_NAME=$OPTARG;;
    r) REMOVE_ENV_IF_EXISTS="y";;
    s) SKIP_UNTRACKED_FILES_CHECK="y";;
    n) CONDA_PACKAGE_NAME=$OPTARG;;
    p) CONDA_PYTHON_VERSION=$OPTARG;;
    \?) usage 1>&2
        exit 1;;
  esac
done

# Info
echo "git describe --tags: $(git describe --tags || true)"
echo "git describe --always: $(git describe --always)"

if [[ -z "$CONDA_PACKAGE_NAME" ]] || [[ -z "$CONDA_PYTHON_VERSION" ]]; then
  >&2 echo "Error: not all mandatory parameters were provided"
  >&2 echo ""
  usage 1>&2
  exit 1
fi

if [[ -z "$CONDA_ENV_NAME" ]]; then
  CONDA_ENV_NAME="${CONDA_PACKAGE_NAME}-build"
fi

CONDA_PACKAGE_PATH="${SCRIPT_DIR}/${CONDA_PACKAGE_NAME}"

if [[ ! -f "${CONDA_PACKAGE_PATH}/meta.yaml" ]]; then
  >&2 echo "File 'meta.yaml' not found: ${CONDA_PACKAGE_PATH}/meta.yaml"
  exit 1
fi

BASE_PACKAGE_PATH="${SCRIPT_DIR}/.."
UNTRACKED_FILES=$(git ls-files "$BASE_PACKAGE_PATH" --ignored --exclude-standard --others | wc -l)
CHANGED_FILES=$(git diff --name-only | wc -l)

if [[ "$UNTRACKED_FILES" -ne "0" ]]; then
  if [[ -z "$SKIP_UNTRACKED_FILES_CHECK" ]]; then
    >&2 echo "Error: there are $UNTRACKED_FILES untracked files in the repository"
    exit 1
  else
    >&2 echo "Warning: there are $UNTRACKED_FILES untracked files in the repository"
  fi
fi

if [[ "$CHANGED_FILES" -ne "0" ]]; then
  >&2 echo "Warning: there are $CHANGED_FILES changed files in the repository"
fi

source $(conda info --base)/etc/profile.d/conda.sh

if [[ -n "$REMOVE_ENV_IF_EXISTS" ]]; then
  conda remove -y -n $CONDA_ENV_NAME --all
fi

if [[ -n "$(conda env list | grep ^$CONDA_ENV_NAME[\ +])" ]]; then
  >&2 echo "Error: conda environment '$CONDA_ENV_NAME' already exists"
  >&2 echo ""
  usage 1>&2
  exit 1
fi

conda create -y -n $CONDA_ENV_NAME -c conda-forge conda-build conda-verify python=$CONDA_PYTHON_VERSION
conda activate $CONDA_ENV_NAME
conda update -y conda

CONDA_CHANNELS="-c conda-forge -c bitextor"

# Make build
conda-build --no-test --no-anaconda-upload $CONDA_CHANNELS $CONDA_PACKAGE_PATH --python=$CONDA_PYTHON_VERSION
