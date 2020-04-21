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

# Symbols, Symbol References, and Aliasing in the OMR compiler

## Introduction

This document explains and documents the concepts and implementation of symbols,
symbol references, and aliasing in the OMR compiler.

## Symbols and Symbol References

The original, conceptual distinction between symbols and symbol references (symrefs)
was actually quite straightforward. However, over time, implementation details have
led to the definition of the two to become very blurry and somewhat unspecified.
This document therefore offers the original definitions, as well as some implementation
details and subtleties.

### Symbols

A symbol represents a memory location. It holds the minimum information required to
directly translate a method's source code or IL representation, without consideration
for optimization. It stores basic information (e.g. data type) needed to determine
what memory access operations can/must be done.

Different kinds of symbols exist to represent the different kinds of memory access
requirements. Each kind of symbol stores information that is specific to its kind (type).
The different symbol kinds are:

* static
   * represents a static memory address
   * contains the address of the memory location
* auto
   * represents a variable mapped on the stack
      * may come from local method variables or compiler allocated stack objects
   * contains the stack offset of the variable
* method
   * represents a method (static, virtual)
* resolved method
* parameter
* register mapped
* label
* shadow
   * represents indirection off a base address
      * e.g. accessing a class field using the class base address plus an offset
   * contains offset from some base address

#### int shadow

An int shadow represents the common case of an integer type. In the case of Java,
for example, an int shadow is created for a field or array access.

#### generic int shadow

A generic int shadow is created by the optimizer in some (relatively rare) cases to
refer to a field that wasn't referred to in the bytecodes/source. If the optimizer wants
to synthesize a load or store, but doesn't have a symbol for it in the original program,
it can create one and call it a generic int shadow.

For example, an escape analysis optimization might want to generate a store to a location
within an object that hasn't been read in this method in order to initialize the location
to zero.

A Generic int shadow has different aliasing depending on the context where it is created
(i.e. it could alias different kinds of other shadows).

### Symbol References

A symbol reference is (or is supposed to be, rather) a wrapper around the different kinds
of symbols. It encapsulates the additional information required by an optimizing compiler,
including:

* aliasing
* owning method (method who's IL created the symbol, which may be different from the
  current method because of inlining)

All symrefs for a given compilation are stored in the "symbol reference table" (symreftab).

Symrefs are numbered, and sets of symrefs are almost always represented as bit vectors where
each bit specifies whether a corresponding symref is a member of the set or not.

## Aliasing

### Basic concept

Aliasing information is associated with symrefs. However, the structural definition of
aliasing used in the OMR compiler differs from the classical definition of aliasing. 
Specifically, it is also used as an indicator of side-effects, rather than to mean 
that multiple names are used to access the same memory. For example, a method symref 
may alias a static symref, signifying that a call to the method kills the value of 
the static.

At a high level, aliasing information is created by calling 
`TR::AliasBuilder::createAliasInfo`. The optimizer infrastructure invokes `createAliasInfo` 
if alias sets have not been built yet or alias sets have been invalidated by a previous 
optimization pass. An optimization pass will invalidate alias sets if it is creating a 
symbol reference, i.e. Inliner, Partial Redundancy Elimination (PRE), 
Escape Analysis (EA), etc. Generally, alias information is not maintained 
by each optimization, and is meant to be rebuilt before a subsequent optimization that 
needs it. An optimization will make queries on the alias sets and decide whether a specific 
transformation should be performed during its pass.

### Asymmetric aliasing

This alternative definition of aliasing permits aliasing relationships to be asymmetric.
To illustrate using the above example, although a method may alias a static, the static
*does not* alias the method because killing the value of the static *does not* affect
the method itself (though it may affect the result returned by a call to the method).

### Low level implementation

At the lowest level, aliasing relations are represented using bit vectors. The bit
vector represents a set of symrefs that alias a particular symref.

The two main APIs for getting a set of aliased symrefs are
`TR::SymbolReference::getUseDefAliasesBV()` and `TR::SymbolReference::getUseonlyAliasesBV`.
When called on a particular symref, they return a bit vector specifying the
aliased symrefs.

The bit vector returned is constructed from various other, more specific bit vectors.
Each of these specifies the symrefs that alias a symref matching certain queries.
Mostly, these queries have to do with the kind of symbol wrapped by the symref. The
associated bit vectors themselves are stored in the `TR::AliasBuilder` class. For a
given symref, each query is successively performed. Every time a query returns a
positive result, the associated bit vector is "ORed" into the final alias bit vector.

In particular, in the case of Java, if `TR::SymbolReference::getUseDefAliasesBV()` is 
invoked on a shadow or static symref, it is likely that the program is doing a resolved 
access store so aliasing information is required. In other words, if it is storing into 
a shadow or static symef, the question is what other symbol references are potentially 
overwritten by the store. Please note that any unresolved accesses (loads or stores) 
can have non-zero alias sets because the resolution process may load a class and call 
clinit, which means the load or store is treated as a method call.

### TR::AliasBuilder

The `TR::AliasBuilder` class is responsible for storing the bit vectors of all symrefs
that alias a symref matching a given query. When a new symref is created, its corresponding
bit must be set in all the bit vectors associated with queries that match the new symref.

These bit vectors are only present for primitive queries. Bit vectors for complex queries
are constructed by `TR:AliasBuilder` by "ORing" together the various bit vectors
associated with queries that compose the larger query.

### TR_AliasSetInterface

The `TR_AliasSetInterface` class is a wrapper around a bit vector representing aliased
symrefs. It provides various methods for performing common operations on these bit vectors.
[The CRTP](https://en.wikipedia.org/wiki/Curiously_recurring_template_pattern) is used
to achieve static polymorphism.

One key interface method that *must* be overridden by sub-classes is `getTRAliases()`.
This is the method responsible for returning the bit vector of aliases.

It is worth noting that some of these operations are templated. So, theoretically, they
can be used on bit vector types different from `TR_BitVector`. However, the type is
required to satisfy the (unspecified) interface of `TR_BitVector`.

Another noteworthy point is that some of the operations rely on calls to functions in
the derived class, as is common with the CRTP. However, because of C++ lazy template
instantiation, these operations will only be materialized if they are called. Hence,
derived classes are not strictly required to define these called functions, as long
as the operations that require them are never called on an instance of this class.

There two classes that inherit from this base class, namely `TR_SymAliasSetInterface`
and `TR_NodeAliasSetInterface`.

### TR_SymAliasSetInterface

`TR_SymAliasSetInterface` acts as a wrapper around a symref and provides an API for
dealing with aliases of the symref. As should be expected, its `getTRAliases()`
method returns the set of aliases of the symref.

Being a class template, it has two specializations with different implementations
of `getTRAliases()`. The non-type template parameter is one of the two values of
the `TR_AliasSetType` enum: `useDefAliasSet` or `UseOnlyAliasSet`. 

### TR_NodeAliasSetInterface

The `TR_NodeAliasSetInterface` is a wrapper around a node (instance of `TR::Node`).
It provides an API for dealing with aliases of the symref *contained* by the node.
That is, its `getTRAliases()` method returns the set of aliases of the symref
contained by the node.

It too is a template class with two specializations having different implementations
of `getTRAliases()`. In this case, the non-type template parameter is one of the two
values of the `TR_NodeAliasSetType` enum: `mayUseAliasSet` or `mayKillAliasSet`.

It should be noted that this class mostly exists for historic reasons. Once upon a time,
aliasing could apply to instances of `TR::Node` as well as symrefs. However, since this
is no longer the case, the class is only used as a shortcut to access the aliases of the
contained symref.

### Aliasing Bit Vectors

Because most of the bit vectors present in the `TR::AliasBuilder` class were originally 
created for Java, discussion in this section will mostly be in the context of Java.

In Java, a callee cannot overwrite an auto or parameter, so aliasing is fairly trivial 
for autos and parameters. Since Java provides type guarantees, for instance integers 
cannot be aliased to floats, we group alias bit vectors by their types. In addition, 
we group them by what kind of symbol references they point to, i.e. statics vs. shadows. 
Therefore, the alias bit vectors can be grouped by the type categories and by the 
storage categories. Within each grouping, the alias bit vectors will never intersect. 
We will discuss a few specific kinds of alias bit vectors below.

#### Generic int shadow bit vector

Generic int shadow bit vector is a relatively simple concept. It is for accessing a field 
of an object but no symbol reference has been made for it. Some optimizations such as 
escape analysis and new initialization operate under this scenario. This alias bit vector 
calculation is very conservative, i.e. aliasing to everything. Because generic int shadow 
is not desirable but needed, we created **generic int array and nonarray shadow bit 
vectors** to distinguish between array and nonarray if we know enough about the base 
class. These two bit vectors are mutually exclusive, i.e. if a symbol reference is in 
the generic int array shadow bit vector, it will not be in the generic int nonarray 
shadow bit vector, but will be included in the generic int shadow bit vector. Generic 
int shadow bit vector will include both generic int array and nonarray shadow bit vectors.

#### Unsafe bit vector

Unsafe bit vector is created for sun.misc.unsafe APIs to fast pass random memory 
accesses and accesses to a field off an object given the base address of the object. 
The unsafe bit vector calculation is fairly conservative, it aliases everything that 
is a shadow or static.

#### gcSafePoint bit vector

gcSafePoint bit vector is created to handle balanced GC mode or real-time mode. During 
these modes, we have pointers to arraylets (with no headers). Marking these pointers as 
collected or not collected won't work, i.e. collected will cause a crash in GC with no 
header information and not collected means we might get garbage after GC. The 
gcSafePoint bit vector is conservative and holds symbol references that need to be 
killed at GC points, for instance at async checks, or allocations, or calls. 
This results in less commoning across GC points.

#### Immutable bit vector

Java has immutable types, i.e. integer, short, byte, long, float, double and string. 
To change their values, you will have to create new ones except in their constructor 
methods. We don't want to include these symbol references across calls such as 
StringBuilder.append(), allowing commoning across the call in this case. We can look 
ahead in other classes and see if they are immutable under higher opt level.

#### catchLocalUse bit vector

catchLocalUse bit vector contains use-only aliases related to exceptions in general. 
It does a reachability analysis from all catch blocks and mark all local variable 
symbol references that can be used. If there is a store before a load for the same 
symbol reference, it will not mark the symbol reference for the load. Shadows and 
statics are not considered in the catchLocalUse bit vector calculation.
