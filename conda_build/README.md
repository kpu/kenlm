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

If you are building for Windows, you will need Visual Studio as well before proceeding.

Once the dependencies are installed, we can create the package:

```bash
conda build --no-anaconda-upload --no-test -c conda-forge ./conda_build
```

When the build finishes, the path to the package should be printed in the log. The path to the package should have the following structure:

```
${CONDA_PREFIX}/conda-bld/{target_arch}/kenlm-{build_date}-{commit}.tar.bz2
```

If you install the generated package (i.e. local installation), the dependencies will not be installed. Be aware that, despite install the dependencies, the constraints might be ignored, what might lead to an inconsistent installation (this does not happen when the package is [uploaded](#upload-package) to the [anaconda repo](https://anaconda.org/anaconda/repo)). Run:

```bash
# Install kenlm (local package)
conda install -y /path/to/package
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
