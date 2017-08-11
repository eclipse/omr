# OMR Checker 

## Prerequisites 

* libclang-dev libraries 
* llvm-dev libraries. 

### Prerequisites for Ubuntu
* llvm
* clang
* make

## Building

### With the Makefile directly

To build OMRChecker, it is enough to simply call `make`. However, if more control is
desired, the following variables can be set:

* `OMRCHECKER_SRCDIR` (defaults to `$PWD`): the directory containing the OMRChecker
source files
* `OMRCHECKER_OBJDIR` (defaults to `$PWD`): the destination directory for the object files
* `OMRCHECKER_OBJECT` (defaults to `$OMRCHECKER_OBJDIR/OMRChecker.so`): the destination
directory for the final shared object file
* `LLVM_CONFIG` (defaults to `llvm-config`): llvm tool to get configuration information
* `CLANG` (defaults to `clang++`): the clang c++ compiler
* `CXX` (defaults to `g++`): standard c++ compiler
* `PYTHON` (defaults to `python2`): the python interpreter (used to run testing script)

The default target will build the OMRChecker shared object. Other predefined targets are:
* `test`: for running a suite of test cases
* `clean`: removes intermediate object files generated
* `cleandll`: removes the OMRChecker shared object
* `cleanall`: removes both the OMRChecker shared object and the intermediate object files

### With the smartmake.sh script

*This is the recommend way of building OMRChecker as part of an automated build/test suite.*

For convenience a short script is provided that automates the process of building the
checker, testing it, and rebuilding if necessary (because a test case failed). To run
the script, simply call `./smartmake.sh`.

In certain cases, when building OMRChecker with an old compiler or on a platform lacking
good clang support, it is possible for the checker to fail to catch any errors. Using
the `smartmake.sh` script helps ensure that the checker produced actually works. It first
tries to build OMRChecker the normal way using the defaults for everything. It then runs
some test to make sure it can catch errors. If all tests pass, the script simply returns 0.
If a test case fails however, it tries to rebuild OMRChecker differently and runs the
tests once again. It then exits with the same return code as the test script (0 on success).

## Using 

Add this to your CXX flags: 

    -Xclang -load -Xclang <path to>/OMRChecker.so -Xclang -add-plugin -Xclang omr-checker

Annotate classes with `__attribute__((annotate("OMR_Extensible")))` (I recommend a macro). 

## Known Weaknesses

1. Layers are identified by string comparison of namespaces. 
2. Self functions are idenfied by string comparisons of namespaces. 
3. There's no support for suggesting `_self`

# Historical interest

Andrew's Original Email:

> To build the plugin grab the llvm-3.4 sources and clang 3.4 sources. To do a
> basic build you extract the llvm sources and then in that extract go to the
> tools directory, make a directory called clang and extract the clang sources in
> there. To do a build make a new directory at the same level as the root of your
> llvm extract and do a `../llvm/configure` and a make in this build directory
> (llvm builds out of tree by default and complains violently if you try to make
> in tree). To build the release+asserts version (much faster without debug
> symbols etc) set `ENABLE_OPTIMIZED=1` on your make line. To build the example
> plugins add `BUILD_EXAMPLES=1` (some thing like `make -j 5 ENABLE_OPTIMIZED=1`
> `BUILD_EXAMPLES=1`).
> 
> The default examples include a plugin called PrintFunctionNames which is
> documented online.
> 
> To build the plugin I wrote, it is probably easiest to just extract the archive
> in both the llvm/tools/clang/examples folder and the
> llvm-build/tools/clang/examples folder and then do a make in the
> llvm-build/tools/clang/examples folder.
> 
> To run the plugin you need to pass arguments to clang's cc1. The basic command
> line I was using was `~/build-llvm/Release+Asserts/bin/clang -Xclang -load
> -Xclang ~/build-llvm/Release+Asserts/lib/libOMRChecker.so -Xclang -add-plugin
> -Xclang omr-checker -fcolor-diagnostics -c Test.cpp`
> 
> The `-load <shared library>` tells clang to bring your shared library into memory
> (doing so runs the plugin registration code at the bottom of the plugin
> implementaiton cpp). The `-add-plugin` says to run the plugin action before
> codegen. Note `-Xclang` is an option that forwards its argument to clang cc1. If
> you only want to run the plugin and not the codegen use `-plugin` instead.
> 
> Building the plugin outside the examples directory etc can be tricky because of
> the comples library linking dependencies between llvm and clang components and
> the comples include paths needed. There is an llvm supplied tool in the tree
> called llvm-config which helps with that, but while you are playing around just
> adding an example is probably the easiest way to go.

