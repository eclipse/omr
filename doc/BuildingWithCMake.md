<!--
Copyright (c) 2016, 2017 IBM Corp. and others

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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0
-->

# Building with CMake

Development is underway for a new build system using CMake. At this point, the CMake build system is **incomplete and
experimental**.

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

### Major feature flags

Compilation of major components is controlled with the following On/Off flags:
* `OMR_COMPILER`: **Not yet supported**.
* `OMR_GC`: Requires `OMR_PORT` and `OMR_THREAD`. Default is `ON`.
* `OMR_JITBUILDER`: **Not yet supported**.
* `OMR_PORT`: Default is `ON`.
* `OMR_THREAD`: Default is `ON`.

Additionally, most testing can be enabled with the `OMR_FVTEST` flag.
For more information on testing, see below.

### The glue directory

The `OMR_GLUE` variable points the build system at the glue directory. It defaults to `example/glue`. The glue directory
is treated as a normal CMake subdirectory.

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
