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

JitBuilder

A library for building JIT compilers, and an advance release of the
Eclipse OMR JIT compiler component
(see [Eclipse OMR Project](https://github.com/eclipse/omr)).

The library can be found in libjitbuilder.a and can be linked
against statically using the API in include/Jit.hpp as well
as using "Builders" : MethodBuilders, IlBuilders, and
BytecodeBuilders.

Some simple examples can be found in the src/ directory, and the
top level directory contains a Makefile that shows you how to
build them.

Just type 'make' and you'll build a number of examples that
demonstrate how you can start building native code dynamically!

More details to come!

This library has been used as the basis of a JIT compiler for
the SOM++ Simple Object Machine runtime.
