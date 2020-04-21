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

SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
-->

# JitBuilder

A library for dynamically generating efficient native code and,
in particular, for building JIT compilers using the Eclipse OMR
JIT compiler component
(see [Eclipse OMR Project](https://github.com/eclipse/omr)).

To use JitBuilder, you create `IlBuilder` objects to represent
the various control flow paths in a function you're trying to create
at runtime. You can create arbitrarily complex control flow by linking
these objects together using services like `ForLoop`, `IfThenElse`,
`Switch` and others. You can then call services on those objects to
append operations to them (to load/store values or to perform operations
on them). It's very easy once you get the hang of it, and you won't
have to learn any of that complicated compiler terminology!

The JitBuilder API also includes a few Vector services for generating
vector instructions and even has a `Transaction` service that makes it
simple to build code that takes advantage of hardware transactional
memory.

And if your goal is to create a JIT compiler for a virtual machine for
some language, the JitBuilder API has services to help you efficiently
model pushes and pops on operand stacks or to access registers in an
operand array that frequently allocate your values to hardware registers
rather than memory locations. There have even been experiments that look
at automatically generating both an interpreter and a JIT compiler for
simple virtual machines using just the definitions of the virtual
machine (VM) state and descriptions of the operations to be performed
by the VM's bytecodes.

JitBuilder can generate efficient native code on X86, POWER, and Z platforms.
Multiple experiments have found the generated JitBuilder code performance
to be comparable or, in some cases, even better than what LLVM can do!
Because it's based on the OMR compiler technology at the heart of the
[Eclipse OpenJ9](https://www.eclipse.org/openj9), it uses memory
efficiently and compiles quickly.

# Steps to build

Here are the basic steps required to build the JitBuilder library and
run the many available code samples from the C++ language.

First, clone the Eclipse OMR repository:

```
$ git clone https://github.com/eclipse/omr
```

Change directory into the OMR project, create a `build` directory, and
generate the makefiles using `cmake` :

```
$ cd omr
$ mkdir build
$ cd build
$ cmake .. -DOMR_COMPILER=1 -DOMR_JITBUILDER=1 -DOMR_TEST_COMPILER=1
```

Now, you're ready to build all the OMR componentry and create the
JitBuilder static library. This step may take a few minutes! It may
go faster if you use `make -jN` with `N` set to the number of cores
on your machine but you can always just type `make` to build the
library:

```
$ make
```

Now that the JitBuilder library has been built, you can change directory
into the `jitbuilder/release` directory which will eventually have different
subdirectories for using the JitBuilder API from different languages. We're
going to use C++, so we'll go into the `cpp` subdirectory:

```
$ cd ../jitbuilder/release/cpp
```

You'll need to create a link to the `libjitbuilder.a` library that you
built. This step should go away some day, but for now you'll need to do
it manually:

```
$ ln -s ../../../build/jitbuilder/libjitbuilder.a .
```

Finally, you can build and run the code samples by running:

```
$ make test
```

If you're running on an x86 platform, then you should be able to run all
28 code samples with:

```
$ make testall
```

That's it! If you want to learn more about how JitBuilder is used, you can
look at the source code for the code samples in the `samples` directory, or
if you want to see how to build code together with the JitBuilder library,
just look for the simple options shown in the `Makefile`.

More tuturial content is coming! For now, you can google "jitbuilder" to find
more presentations and articles that have been written about this library!

# Projects using JitBuilder

JitBuilder has been used in a number of different places, from a
standalone JIT compiler for [WebAssembly](https://github.com/wasmjit-omr/wasmjit-omr)
to an experimental JIT compiler for the
[Rosie Pattern Language](https://github.com/mstoodle/rosie-lpeg/tree/experimental_omrjit)
to Smalltalk and beyond. There is even an LLVM IR to JitBuilder converter
currently under construction at https://github.com/nbhuiyan/lljb !
