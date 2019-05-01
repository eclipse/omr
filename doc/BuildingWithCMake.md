<!--
Copyright (c) 2016, 2018 IBM Corp. and others

This program and the accompanying materials are made available under
the terms of the Eclipse Public License 2.0 which accompanies this
distribution and is available at https://www.eclipse.org/legal/epl-2.0/
or the Apache License, Version 2.0 which accompanies this distribution and
is available at https://www.apache.org/licenses/LICENSE-2.0.

This Source Code may also be made available under the following
Secondary Licenses when the conditions for such availability set
forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
General Public License, version 2 with the GNU Classpath 
Exception [1] and GNU General Public License, version 2 with the
OpenJDK Assembly Exception [2].

[1] https://www.gnu.org/software/classpath/license.html
[2] http://openjdk.java.net/legal/assembly-exception.html

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# Building with CMake

Normal build instructions are in the main [`README.md`](../README.md).

## Requirements

* `cmake` 3.2 or newer;
* A supported C++ toolchain, E.G. `clang`, `gcc`, or `msvc`;
* A supported build tool, E.G. `ninja`, `make`, or Microsoft Visual Studio.

## Alright, lets do this

```bash
# always prefer building outside the source tree
mkdir -p build && cd build
# configure the build
cmake .. # point cmake at the root source directory
# build it
cmake --build .
```

## Configuring the build

CMake build variables can be set on the command line with `cmake -D<flag>[=<value>]`. Variables are cached in
`CMakeCache.txt`. You can list cached variables with `cmake -L`. There is also a helpful tool, `cmake-gui`, which may
make your life easier.

CMake can generate build files for a number of backends. You can select the generator with `-G<generator>`, and list
available generators with `--help`. We recommend using with the `Ninja` generator, when available. More information is
available [here](https://cmake.org/cmake/help/latest/manual/cmake-generators.7.html).

### Configuring the build on Windows

There are three widely used C/C++ toolchains for Windows: *MSYS2/MinGW*, *Microsoft Visual Studio* and *LLVM/Clang*.
Let's consider how to use each toolchain to configure and build `OMR` with `CMake`.

#### Common prerequisite

*Perl* must be installed to configure the project. [Strawberry Perl](http://strawberryperl.com/) is absolutely
suitable for `CMake` on Windows, please do not forget to add the path to `perl.exe` (for example 
`C:\StrawberryPerl\perl\bin`) to the `PATH` environment variable.

#### Configuring the build for using MSYS2/MinGW

The code of `OMR` uses the Microsoft extension to `C` for supporting *Windows Structured Exception Handling
([SEH](https://msdn.microsoft.com/library/windows/desktop/ms680657))*. *MinGW* haven't supported it
for a long time. Some workarounds exist but none of them is suitable to use in production-quality software.

#### Configuring the build for using Visual Studio Build Tools

*Visual Studio Build Tools* is a bundle with a standalone compiler, libraries and scripts to build C++ libraries and
applications targeting Windows desktop. The [build tools](http://landinghub.visualstudio.com/visual-cpp-build-tools)
are available for download on Microsoft's website.

The following components of the build tools are recommended to be installed:

 * Windows 10 SDK (>= 10.0.16299.0) for Desktop C++
 * Visual C++ tools for CMake

**Note:** 
To use the `NMake` or `Ninja` generator with *Visual C++*, `cmake` must be run from a shell that can use 
the compiler `cl` from the command line as it described in the document
[Build C/C++ code on the command line](https://docs.microsoft.com/en-us/cpp/build/building-on-the-command-line).
To enable the shell, please run
`C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\VC\Auxiliary\Build\vcvarsall.bat [platform]` where `platform`
can be one of the following options: `x86_amd64`, `x86_arm uwp`, `x86_arm onecore`, `x64 8.1` or `x64 store 8.1`.

**Note:**
If you wish to build the `DDR` component, `CMake` should be able to find the path to the `DIA SDK`. There are two ways to specify
the path:

 * Create the `VSSDK140Install` environment variable and set its value to the path to the *Visual Studio Build Tools* installation,
for example, `C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools`.

 * Create the `DIASDK` environment variable and set its value to the path to the `DIA SDK` folder itself, for example,
`C:\Program Files (x86)\Microsoft Visual Studio\2017\BuildTools\DIA SDK`.

Then, to configure the build, let's do this:

```bash
# always prefer building outside the source tree
mkdir -p build && cd build

# configure the build using Ninja
cmake -DCMAKE_CXX_COMPILER=cl -DCMAKE_C_COMPILER=cl .. -G"Ninja"

# ... or using NMake
cmake -DCMAKE_CXX_COMPILER=cl -DCMAKE_C_COMPILER=cl .. -G"NMake Makefiles"

# ... and build it
cmake --build .
```

#### Configuring the build for using Clang

*Clang* is released as part of regular *LLVM* releases. You can download the release versions from the
[LLVM Download Page](http://releases.llvm.org/download.html). *Clang* attempts to be compatible with *MSVC* and provides a
special version of compiler: `clang-cl`. A good news is here: the compiler can be used to build `OMR` for Windows.

**Note:**
`clang-cl` requires the same libraries and tools as `MSVC` does, so the
[previous section](#configuring-the-build-for-using-visual-studio-build-tools) should be taken into account:
*Visual Studio Build Tools* must be installed and `cmake` must be run from the *Visual Studio Command Prompt* (`vcvarsall.bat`).
The `PATH` environment variable has to contain the `clang-cl` location.

After preparing the environment, let's do this:

```bash
# always prefer building outside the source tree
mkdir -p build && cd build

# configure the build using Ninja
cmake -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER=clang-cl .. -G"Ninja"

# ... or using NMake
cmake -DCMAKE_CXX_COMPILER=clang-cl -DCMAKE_C_COMPILER=clang-cl .. -G"NMake Makefiles"

# ... and build it
cmake --build .
```

Please note that `Clang` is more rigorous than `MSVC` and generates much more warnings and occasionally even can not
compile the code that correctly compiled by using its sibling from Microsoft.

### Major feature flags

Compilation of major components is controlled with the following On/Off flags:
* `OMR_COMPILER`: **Not yet supported**.
* `OMR_GC`: Requires `OMR_PORT` and `OMR_THREAD`. Default is `ON`.
* `OMR_JITBUILDER`: **Not yet supported**.
* `OMR_PORT`: Default is `ON`.
* `OMR_THREAD`: Default is `ON`.

Additionally, most testing can be enabled with the `OMR_FVTEST` flag.
For more information on testing, see below.

### The glue libraries

With the move to CMake the glue as been separated out into a number of components, specified by the following variables:
* `OMR_GC_GLUE_TARGET`
* `OMR_UTIL_GLUE_TARGET`
* `OMR_RAS_GLUE_TARGET`
* `OMR_CORE_GLUE_TARGET`

Each variable should specify the name of an interface library, which will be linked into the corresponding OMR library.
Eg. `OMR_GC_GLUE_TARGET` will be linked into `omrgc`, `OMR_UTIL_GLUE_TARGET` will be linked into `omrutil` and so on.

## Running tests

Ensure the tests are being built and that `OMR_FVTEST` is `On` (Note: default is off).

You must use the example glue. `OMR_GLUE` should be `example/glue`. `OMR_EXAMPLE` should be set to `ON`. Most tests will
not function without the example glue.

To actually run the tests:
```bash
# from the build directory
ctest -V
```

## Cross compiling OMR

**Cross compiling is untested.** You must define a custom toolchain file for you cross compilers. More information about
this is [here](https://cmake.org/cmake/help/v3.0/manual/cmake-toolchains.7.html). OMR relies on native build tools that
must be available on the build platform. In a cross compile, these tools are to be imported from a native build.

Cross compiling OMR is done in two steps:

1. build the tools targeting the native platform:
   ```bash
   mkdir -p native_build && cd native_build
   cmake .. && cmake --build .
   ```
   This will generate `tools/ImportTools.cmake`. To import the tools, point the variable `OMR_TOOLS_IMPORTFILE` at this
   file.

2. Cross compile using your CC toolchain and native tools:
   ```bash
   mkdir -p foreign_build && cd foreign_buid
   cmake .. -DCMAKE_TOOLCHAIN_FILE=path/to/file
   cmake .. -DOMR_TOOLS_IMPORTFILE=../native_build/tools/ImportTools.cmake
   cmake --build .
   ```

## Extra Feature Flags

*  OMR_SANITIZER={address,thread,undefined} turns on one: [Address Sanitizer
   (ASan)][asan], [Undefined Behaviour Sanitizer (UBSan)][ubsan] or [Thread
   Sanitizer (TSan)][tsan]. This only has an effect with GCC or Clang. 

[ubsan]: https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html
[asan]: https://github.com/google/sanitizers/wiki/AddressSanitizer
[tsan]: https://clang.llvm.org/docs/ThreadSanitizer.html 

## What's next?

The work here is ongoing. We are tracking development with github issue
[#993](https://github.com/eclipse/omr/issues/933).
