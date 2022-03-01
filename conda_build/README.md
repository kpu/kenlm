# Conda build

Instructions in order to create, install and upload a conda package. kenlm is available on Windows, Ubuntu and Mac, so the package is it as well.

You will need to [download](https://docs.conda.io/en/latest/miniconda.html) and [install](https://conda.io/projects/conda/en/latest/user-guide/install/index.html) Miniconda.

## Create package

First, you will need to install some dependencies:

```bash
conda install -y -c conda-forge conda-build
# Mandatory for Windows, optional otherwise
conda install -y -c conda-forge git
```

Once the dependencies are installed, we can create the package:

```bash
conda build --build-only -c conda-forge ./conda_build
```

When the build finishes, the path to the package should be printed in the log. The path to the package should have the following structure:

```
${CONDA_PREFIX}/conda-bld/{target_arch}/kenlm-{build_date}-{commit}.tar.bz2
```

If you install the generated package, the dependencies will not be installed. Run:

```bash
# Install kenlm
conda install -y ${CONDA_PREFIX}/conda-bld/{target_arch}/kenlm-{build_date}-{commit}.tar.bz2
# Install dependencies
conda update -y --only-deps -c conda-forge kenlm
```

## Upload package

If you want to upload your package, install:

```bash
conda install -y -c conda-forge anaconda-client
```

In order to upload the package, we just need to execute the following command and follow the instructions:

```bash
anaconda upload /path/to/package
```
