# Programming OMR IL

## Introduction

The goal of this document is to describe how to program with OMR IL. We hope to cover:

* Intended Audience
* What is IL
* Where is it?
* Getting started
* Basic IL Generation Flow
* Key OMR IL concepts
* Blocks
* Nodes and TreeTops
* Stack allocated data
* Using data across Blocks
* Branching and CFGs
* Other Information Sources

## Intended Audience

This document is aimed at software engineers who want to use the OMR JIT technology in their front-end, in particular those who want
to understand and work at a lower level than the JitBuilder api offers. This document is not intended to be a reference for IL, nor does it
cover how to program using the JitBuilder api. This document is also not meant for backend engineers.

## What is IL

Eclipse OMR uses [DAGs](https://github.com/dibyendumajumdar/dmr_c/tree/master/omrjit-backend) to represent
intermediate code. I guess IL stands for Intermediate Language. In contrast LLVM uses linear SSA IR as the intermediate language.

The Intermediate Language is a representation of code that is independent of the front-end programming
language such as Java or C. By using an IL, the compiler can support multiple front-end programming 
languages and multiple backend machine code generators with less effort and the IL bridges the two worlds.

## Where is it?

This may sound a strange question to ask, but one that I had to struggle with initially. This is because
most of the OMR presentations and talks are about the [JitBuilder api](https://developer.ibm.com/open/2016/07/19/jitbuilder-library-and-eclipse-omr-just-in-time-compilers-made-easy/) which 
is probably the easiest way to get started with the OMR JIT engine. However, JitBuilder hides a lot of the details
so you are not really working with OMR IL directly when you use JitBuilder. This document is mainly about how to
work directly with the IL layer.

The main IL classes that you will need to use are in [compiler/il](https://github.com/eclipse/omr/tree/master/compiler/il) 
sub-directory. Some of the classes there will be discussed below.

The other location of interest is [compiler/ilgen](https://github.com/eclipse/omr/tree/master/compiler/ilgen) which is
where most of the JitBuilder classes live, along with some lower level interface definitions.

## Getting Started

OMR compiler backend code is not available as a standalone library due to [the way OMR extensibility works](https://github.com/eclipse/omr/blob/master/doc/compiler/extensible_classes/Extensible_Classes.md). Code that wishes
to use the IL directly has to include the compiler sources and must provide a few required extension classes. 
There is also some CMake machinery to help with the building of a component that requires the compiler sources; if you look at the 
[CMake configuration for JitBuilder](https://github.com/eclipse/omr/blob/master/jitbuilder/CMakeLists.txt), this will
give you an idea of how this works. 

You can however build JitBuilder as a standalone library; the JitBuilder library includes the compiler
backend obviously. Having the JitBuilder component is convenient also because you can build upon some of the scaffolding it 
provides. Note however that you will need to include header files within the OMR project as there isn't a set of
header files that are distributed.

I found that if you want to create your own OMR library then the easiest option is to add your sources to the `jitbuilder`
component so that it gets built as part of the JitBuilder library.

If you are writing executable programs it is convenient to model your project on [Tril](https://github.com/eclipse/omr/tree/master/fvtest/tril)
which is an nice little testing library that allows IL generation [using a lisp like syntax](https://github.com/eclipse/omr/blob/master/fvtest/tril/examples/mandelbrot/mandelbrot.tril). Alternatively look
at the [JitBuilder samples CMake configuation](https://github.com/eclipse/omr/blob/master/jitbuilder/release/CMakeLists.txt).

The CMake build instructions for OMR can be found at [omr/doc/BuildingWithCMake.md](https://github.com/eclipse/omr/blob/master/doc/BuildingWithCMake.md). Note that by default the JitBuilder
library is not built; to enable that you need to provide `-DOMR_JITBUILDER=ON` option to CMake. 

The OMR code is written in C++; as yet there is no official C api for the IL layer. A C api for JitBuilder
is [in the works](https://github.com/eclipse/omr/issues/2397).

If it sounds like it is a bit difficult to get started if you want to work directly with OMR IL, then that is true at this 
point in time. However the setup is a one time effort so don't let that put you off.

## Basic IL Generation Flow

At a high level the flow is as follows:

* You need to define the function you want to create. This is done by creating an instance of [`TR_Method`]
  (https://github.com/eclipse/omr/blob/master/compiler/compile/OMRMethod.hpp). `TR_Method`
  defines the function's parameters and return type, and is used to resolve any function, not just the ones you JIT compile.
  JitBuilder provides a derived type called `TR::ResolvedMethod` which can also be used as the basis.
* Next you need to create an instance of [`TR_IlGenerator`](https://github.com/eclipse/omr/blob/master/compiler/ilgen/IlGen.hpp).
  The compiler backend will invoke the `TR_IlGenerator::genIL()` method when it wishes you to generate the IL for the function. 
  Again it is convenient to use a derived class [`TR::ILInjector`](https://github.com/eclipse/omr/blob/master/compiler/ilgen/IlInjector.hpp) as a starting point for your own type. This class will give you an idea of what you need as a minimum.
* As a next step you ask the backend to compile the function. For this purpose you can call [`compileMethodFromDetails()`]
  (https://github.com/eclipse/omr/blob/master/compiler/control/CompileMethod.hpp). This is when the actual IL generation starts.
  The compiler backend sets up a Compiler object which is saved in a thread local variable; this is why when you call one of
  Node creation methods (to be discussed later) it knows which compiler object to hook into. During IL generation the `genIL()` method
  is called which will in turn run any code you have defined. At the end of the compilation process you are given a pointer to
  the compiled function.
* You have to manage the pointer to compiled function somewhere as although the compiled code is saved in a Code Cache internally
  there is no api to access it directly. Typically you will want to associate the compiled function to a name, and possibly also the
  `TR_Method` instance you created.

I hope to make all of above clearer through an example (yet to be written!)

