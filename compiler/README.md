<!--
Copyright (c) 2016, 2019 IBM Corp. and others

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

# Eclipse OMR Compiler Technology

The Eclipse OMR compiler technology is a collection of stable, high performance optimization and code generation technologies suitable for integration into dynamic and static language runtime environments.

It is not a standalone compiler that can be linked into another environment.  Rather, it provides all the essential building blocks for integrating and adapting this advanced compiler technology for different language environments.

It originates from a mature, feature-rich compiler technology developed within IBM called Testarossa.  This technology was developed from the outset to be used in highly dynamic environments such as Java, but has proven its adaptability in static compilers, trace-based compilation, and binary re-translators.

This technology is consumed as-is in some of IBM's high-performance runtimes such has the J9 Java virtual machine and used in production environments.

# What has been contributed?

* high-level optimization technology featuring classic compiler optimizations, loop optimizations, control and data flow analyses, and support data structures
* code generation technology with deep platform exploitation for x86 (i386 and x86-64), Power, System Z, and ARM (32-bit)
* a robust, tree-based intermediate representation (or IL) and support code for producing IL from different method representations
* expressive tracing and logging infrastructure for problem determination
* [JitBuilder](https://developer.ibm.com/open/2016/07/19/jitbuilder-library-and-eclipse-omr-just-in-time-compilers-made-easy/) technology to simplify the effort to integrate a JIT compiler into an existing language interpreter
* a framework for constructing language-agnostic unit tests for compiler technology

# What's next?

While this is a significant initial contribution of compiler technology, more is on the way, including:

* integration of conformance tests with the Eclipse OMR makefiles, also demonstrating a sample hookup of the technology
* lots more documentation, in source and in the `doc/compiler` directory describing the compiler technology architecture and its inner workings
* further code refactoring and design consistency enhancements
* additional optimizations and code generation technology upon refactoring from within the IBM Testarossa code


# A Tour of the Source Code

The compiler technology is written largely in C++, but there is a handful of support functions written in C and assembler.

The structure of the codebase and the design of the class hierarchy reflects this technology's heritage and the requirement to adapt to a wide variety of compilation environments (or *projects* as they are often referred).

Many of the classes in Testarossa use a design pattern we call [*extensible classes*](../doc/compiler/extensible_classes/Extensible_Classes.md).  This is a pattern to achieve extension through composition and static polymorphism.
Extensible classes are an efficient and useful means to extend and specialize the core technology provided by Eclipse OMR for a particular project and for a particular processor architecture.
The extensible design is the reason for the shape of the class hierarchy, the layout of directories, and file naming.

The core compiler components are provided under the `compiler/` top-level directory and are organized as follows:

Directory  | Purpose
---------  | -------
codegen/   | Code for transforming IL trees into machine instructions.  This includes pseudo-instruction generation with virtual registers, local register assignment, binary encoding, and relocation processing.
compile/   | Logic managing the compilation of a method.
control/   | Generic logic to decide on when and how to compile a method.
cs2/       | A legacy collection of utilities providing functionality such as container classes and lexical timers.  The functions within this directory are **deprecated** and are actively being replaced with C++ STL equivalents or new implementations based on STL.
env/       | Generic interface to the environment that is requesting the compilation.  In most cases this is the interface to the VM or compiler frontend that is incorporating the Eclipse OMR compiler technology.  For example, it can be used to answer questions about the VM configuration, object model, classes, floating point semantics, etc.
il/        | Intermediate language definition and utilities.
ilgen/     | Utilities to help with the generation of intermediate language from some external representation.
infra/     | Support infrastructure.
optimizer/ | High-level, IL tree-based optimizations and utilities.
ras/       | Debug and servicability utilities, including tracing and logging.
runtime/   | Post-compilation services available to compiled code at runtime.
aarch64/   | AArch64 processor specializations
arm/       | ARM processor specializations
x/         | X86 processor specializations
p/         | Power processor specializations
z/         | System Z processor specializations

Other resources can be found in the Eclipse OMR project as follows:

Directory | Purpose
--------- | -------
jitbuilder/ | JitBuilder technology extending Eclipse OMR technology
fvtest/compilertest | Unit tests for compiler technology
doc/compiler | Additional documentation

## Namespaces

The `OMR`, `TestCompiler`, and `JitBuilder` namespaces are used to isolate
compiler technology for those particular components.  Processor architecture
specialized namespaces (e.g., `X86`, `Power`, `Z`, and `ARM`) can be
nested within them, and secondary nesting for sub-architectures (e.g., `I386`,
`AMD64`) is also permitted.

The `TR` namespace (an abbreviation for Testarossa) should be used for all
components of the OMR compiler that are part of the public API for consuming
projects.  The OMR compiler public API includes the concrete classes for the
extensible class representation (see [*extensible classes*](../doc/compiler/extensible_classes/Extensible_Classes.md)).

At present, the use of the `TR` namespace for the public API is largely aspirational
as much of the code appears as it did when it was first contributed.  The epic to
track the work to migrate components of the OMR compiler public API to the `TR`
namespace is issue [#3519](https://github.com/eclipse/omr/issues/3519).
Throughout the current compiler code, you may encounter references that are in
the global namespace but whose identifiers are prefixed simply with `TR_`.
This is inconsistent with the namespace convention just described and they are
being moved to the `TR` namespace as part of #3519.

If you extend the Eclipse OMR compiler you should choose a unique namespace
for your project that does not conflict with the compiler namespaces.

## XXX_PROJECT_SPECIFIC macros

Throughout the codebase you may find code guarded with `#ifdef XXX_PROJECT_SPECIFIC` directives.  Project-specific macros are an artifact of the refactoring process that produced this code.  The initial code contribution originated from a much larger codebase of compiler technology used in several compiler products and compilation scenarios.  In order to contribute this code sooner we eliminated most, but not all, of code that has a tighter coupling to a particular environment.  For example, guarded code may require data structures or header files not present in the initial contribution.

Generally there should not be a need to enable these macros.  Our expecatation is that either that guarded code will be enabled over time as it is made more general purpose for other language environments, or it will be removed outright as that code is refactored as part of the Eclipse OMR project.

We recognize the presence of these macros is far from ideal, and IBM will be working to eliminate them over time so that the codebase is self-contained and fully testable.  These macros should not be imitated in newer code commits except where absolutely necessary.

