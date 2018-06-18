# Programming OMR IL

## Introduction

The goal of this document is to describe how to program with OMR IL. We hope to cover:

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

## What is IL

Eclipse OMR uses [DAGs](https://github.com/dibyendumajumdar/dmr_c/tree/master/omrjit-backend) to represent
intermediate code. I guess IL stands for Intermediate Language. In contrast LLVM uses linear SSA IR as the intermediate language.

The Intermediate Language is a representation of code that is independent of the front-end programming
language such as Java or C. By using an IL, the compiler can support multiple front-end programming 
languages and multiple backend machine code generators with less effort and the IL bridges the two worlds.

## Where is it?

This may sound a strange question to ask, but one that I had to struggle with initially. This is because
most of the OMR presentations and talks are about the [JitBuilder api](https://developer.ibm.com/open/2016/07/19/jitbuilder-library-and-eclipse-omr-just-in-time-compilers-made-easy/) which 
is probably the easiest way to get started with the JIT engine. However, JitBuilder hides a lot of the details
so you are not really working with OMR IL directly when you use JitBuilder. This document is mainly about how to
work directly with the IL layer.

The main IL classes that you will need to use are in [compiler/il](https://github.com/eclipse/omr/tree/master/compiler/il) 
sub-directory. Some of the classes there will be discussed below.

The other location of interest is [compiler/ilgen](https://github.com/eclipse/omr/tree/master/compiler/ilgen) which is
where most of the JitBuilder classes live, along with some lower level interface definitions.

## Getting Started

OMR compiler backend code is not available as a standalone library due to [the way OMR extensibility works](https://github.com/eclipse/omr/blob/master/doc/compiler/extensible_classes/Extensible_Classes.md). Code that wishes
to use the IL directly has to include the compiler sources. This is done via some CMake machinery; if you look at the 
[CMake configuration for JitBuilder](https://github.com/eclipse/omr/blob/master/jitbuilder/CMakeLists.txt), this will
give you an idea of how this works. 

In practice it is convenient to model your project like [Tril](https://github.com/eclipse/omr/tree/master/fvtest/tril)
which is an nice little testing library that allows IL generation [using a lisp like syntax](https://github.com/eclipse/omr/blob/master/fvtest/tril/examples/mandelbrot/mandelbrot.tril).
Also having the JitBuilder component is convenient because you can build upon some of the scaffolding it provides.

The CMake build instructions for OMR can be found at [omr/doc/BuildingWithCMake.md](https://github.com/eclipse/omr/blob/master/doc/BuildingWithCMake.md). 



