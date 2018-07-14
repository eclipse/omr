# Programming OMR IL

## Introduction

The goal of this document is to describe how to program with OMR IL. We hope to cover following topics:

* Intended Audience
* What is IL
* Where is it?
* Getting started
* Basic IL Generation Flow
* IL
* Data
* Branching and CFGs
* Example

## Intended Audience

This document is aimed at software engineers who want to use the OMR JIT technology in their front-end, in particular those who want
to understand and work at a lower level than the JitBuilder api offers. This document is not intended to be a reference for IL, nor does it
cover how to program using the JitBuilder api. This document is also not meant for backend engineers.

## What is IL

Eclipse OMR uses [DAGs](https://en.wikipedia.org/wiki/Directed_acyclic_graph) to represent
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
where most of the JitBuilder classes live, along with some lower level interface definitions. The following source files are
really part of JitBuilder api - and if you want you can write code against IL without using these.

* `compiler/ilgen/OMRIlBuilder.cpp`
* `compiler/ilgen/OMRIlType.cpp`
* `compiler/ilgen/OMRIlValue.cpp`
* `compiler/ilgen/IlInjector.cpp`
* `compiler/ilgen/OMRMethodBuilder.cpp`
* `compiler/ilgen/OMRBytecodeBuilder.cpp`
* `compiler/ilgen/OMRTypeDictionary.cpp`
* `compiler/ilgen/OMRThunkBuilder.cpp`		
* `compiler/ilgen/OMRVirtualMachineOperandArray.cpp`		
* `compiler/ilgen/OMRVirtualMachineOperandStack.cpp`		

## Getting Started

The OMR compiler backend code is not available as a standalone library due to [the way OMR extensibility works](https://github.com/eclipse/omr/blob/master/doc/compiler/extensible_classes/Extensible_Classes.md). Code that wishes
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

* You need to define the function you want to create. This is done by creating an instance of [`TR_Method`](https://github.com/eclipse/omr/blob/master/compiler/compile/OMRMethod.hpp). `TR_Method`
  defines the function's parameters and return type, and is used to resolve any function, not just the ones you JIT compile.
  JitBuilder provides a derived type called `TR::ResolvedMethod` which can be used as the basis. You can roll out your own
  version too if you want - but for now let's assume we are using the JitBuilder provided class.
* Next you need to create an instance of [`TR_IlGenerator`](https://github.com/eclipse/omr/blob/master/compiler/ilgen/IlGen.hpp).
  The compiler backend will invoke the `TR_IlGenerator::genIL()` method when it wishes you to generate the IL for the function. 
  It is convenient to use a derived class [`TR::ILInjector`](https://github.com/eclipse/omr/blob/master/compiler/ilgen/IlInjector.hpp) as a starting point for your own type. This class will give you an idea of what you need as a minimum.
* As a next step you ask the backend to compile the function. For this purpose you can call [`compileMethodFromDetails()`](https://github.com/eclipse/omr/blob/master/compiler/control/CompileMethod.hpp). This is when the actual IL generation starts.
  The compiler backend sets up a Compiler object which is saved in a thread local variable; this is why when you call one of
  Node creation methods (to be discussed later) it knows which compiler object to hook into. During IL generation the `genIL()` method
  is called which will in turn run any code you have defined. At the end of the compilation process you are given a pointer to
  the compiled function.
* You have to manage the pointer to compiled function somewhere because, although the compiled code is saved in a Code Cache internally
  there is no api AFAIK to access it directly. Typically you will want to associate the compiled function to a name, and possibly also the
  `TR::ResolvedMethod` instance you created.

I hope to make all of above clearer through examples.

## IL 

* Firstly the IL is represented as DAGs rather than linear IR.
* The main unit of IL instruction is a Node. A Node is a DAG so it can have one of more children (Nodes) and (tbc) one or more 
  parents (also Nodes).
* There is a natural relationship between Nodes due to above, but additionally TreeTops are used to anchor Nodes at certain points.
  I think of TreeTops as equivalent to statement boundaries in a high level language. More on these later.
* Nodes and TreeTops are grouped into Blocks which are really Basic Blocks. A Basic Block contains a sequences of TreeTops and 
  associated Nodes and always ends with a IL Node that represents a branch or return instruction. 
* OMR also requires that you setup a Control Flow Graph - which is a graph that is at the level of Blocks. Basically as you add
  branching you need to also tell OMR about the edge from the source Block to destination Block.

## Data

In addition to IL instructions your program will need to deal with data such as local variables, parameters, etc. 

* A `Symbol`/`SymbolRef` is required for any data item
* You can roughly group symbols into:
  - Parameters
  - Scalar values  
  - Aggregate values (such as C structs or arrays)
* There are IL instructions to `load` a value from a symbol or `store` a value to a symbol.  
* One of the rules regarding values is that you cannot access a value defined in one block from another directly, it must go
  via a `store`/`load` operation. Since the IL is not an SSA IR you don't have phi instructions for this; but you get the same effect
  by performing a `store` in one block and a `load` in another.

## Example

Our goal here to demonstrate the process of using IL through a simple example. I want to pick an example that illustrates all the
aspects of OMR IL mentioned above. We will construct this example step by step.

### Setup as a JitBuilder sample project

JitBuilder samples are located under [`jitbuilder/release`](https://github.com/eclipse/omr/tree/master/jitbuilder/release).
For a simple example, we can add the source in the `src` folder and update the `CMakeLists.txt` to build our example.
Note that although we are using the JitBuilder samples project as our home for the example, it does not need to be.
The only reason for doing it this way is to avoid having to setup some of the extension classes we would otherwise need; 
instead here we will just use the classes provided by JitBuilder. However when it comes to coding the IL, our aim is to
directly use the lower level api and understand what is going on.

