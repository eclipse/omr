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

# Tril

Tril is a Domain Specific Language (DSL) for generating the Testarossa
Intermediate Language (IL). Its goal is to simplify testing Testarossa by
providing very fine control over the IL that is fed into the compiler.

## Overview

The Tril implementation is a small library with a parser and IL generator,
located in the `tril/` directory. It provides the core tools for dealing with
Tril code. Compilation and executing of Tril code is implemented separately.
This allows custom tools to be easily developed on top of Tril.

The `examples/` directory contains examples demonstrating how to implement
compilation and execution of Tril code.

The `test/` directory contains some GTest-based test cases for Tril.

## Building Tril

1. Make sure you have the latest versions of cmake, flex, and yacc installed
on your machine

2. Clone the Eclipse OMR repo:

    ```sh
    git clone https://github.com/eclipse/omr.git
    ```

3. Build tril using `cmake`. To ensure OMR gets built in a compatible
   configuration, we currently recommend you use the CMake cache that drives
   our TravisCI builds.

    ```sh
    mkdir build
    cd build
    cmake -C../cmake/caches/<spec_name>.cmake ..
    ```

    At this point you can use whatever generated build system to build
    tril: 

    ```
    make tril
    ```

5. You should now have a static archive called `libtril.a`

6. You can build the tests by building `triltest`, or the compiler tests built
   using tril by by building `comptest`. There's also the `mandelbrot` and `incordec` 
   examples.

4. Enjoy the view!

   ```
   ./fvtest/tril/examples/mandelbrot/mandelbrot ../fvtest/tril/examples/mandelbrot/mandelbrot.tril
   ```
