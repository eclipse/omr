# Programming OMR IL

## Introduction

The goal of this document is to describe how to program with OMR IL. We hope to cover:

* What is IL
* Key OMR IL concepts
* Getting started
* Blocks
* Nodes and TreeTops
* Stack allocated data
* Using data across Blocks
* Branching and CFGs

## What is IL

Eclipse OMR uses [DAGs](https://github.com/dibyendumajumdar/dmr_c/tree/master/omrjit-backend) to represent intermediate code. I guess
IL stands for Intermediate Language. In contrast LLVM uses linear SSA IR as the intermediate language.

The Intermediate Language is a representation of code that is independent of the front-end programming language such as Java or C.
By using an IL, the compiler can support multiple front-end programming languages and multiple backend machine code generators with
less effort and the IL bridges the two worlds.

