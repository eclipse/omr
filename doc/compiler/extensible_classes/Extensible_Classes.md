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

# Extensible Classes {#ExtensibleClasses}

## Introduction

Extensible classes are a way of implementing static polymorphism in C++. They
allow various projects to override and extend the functionality of an existing
class, through inheritance, in order to suit their specific needs. This is done
by carefully organizing a project in such a way that the compiler can find the
most extended implementation of a member function, no matter where it is called
from.

This document describes the general characteristics of extensible classes as
well as their implementation. Alternate approaches to this problem are also
explored and compared to extensible classes.

## What is an extensible class?

Suppose we have a group of projects that are similarly structured. They share
similar features and have a similar implementation.  It makes sense to abstract
away the common functionality and structure so that it may be reused more
effectively.

Extensible classes organize code in a way that allows the structure and
implementation of a particular class to be extended.  Conceptually, when
extending a class, we do not create a new type. Instead, we take a previously
implemented type and extend its functionality to suit our needs. We can access
an instance of an extended class through a reference to the non-extended
version because they are actually the same type (though doing so also hides the
extended functionality). It is therefore common to refer to an entire
extensible class hierarchy as a single class.

In addition, connectors allow extensible classes to define specialized
extensions for use in specific situations. These can be very useful, for
example, in cases where some special functionality is required to support a
specific hardware architecture (a common problem in compiler development).

> Traditionally, C++ solves these problems using dynamic polymorphism through
the use of the `virtual` keyword. However, extensible classes allow for greater
efficiency.

## Implementation

In essence, an extensible class is just a hierarchy of normal C++ classes. What
makes extensible classes special is how project files, directories, and class
hierarchies are organized.

### Organization

Extensible classes organize the implementation of a class into a few distinct
parts. These are the class extensions, extension specializations, and the
concrete class.

A class extension is a class that extends the implementation of another class
extension. Class extensions are abstract: they cannot be instantiated; only
extended. An extension inherits the functionality of the parent extension and
adds its own functionality to it (*extends* it). It can also *override* the
parent's functionality.

A special case of a class extension is the base class that does not have a
parent of its own, but still is extended by other classes. Although it does not
extend the structure or functionality of any existing class, it is treated the
same as other class extensions (notably, it is abstract).

Concrete classes are at the bottom of an extensible class hierarchy. They
realize the implementation of the class extension they inherit from. These are
the only classes in an extensible hierarchy that can be instantiated.

Extension specializations are special class extensions that specialize the
functionality of a particular class extension. They allow us to define different
implementations of the same class extension. We can then force the compiler to
use a specific set of extension specializations. This is particularly useful
for organizing platform specific code.

It should be noted that this must be a linear hierarchy at compile time. This
means each class may only have one parent, so multiple inheritance is not
allowed. This also means there can only be one child of each class extension
visible to the compiler. There may be more children of a class extension if
they are specializations, but only one must be visible at compile time.

### self()

The fundamental principle of extensible classes is to let the compiler find the
most derived (extended) implementation of member functions at compile time. To
do this, the compiler must search the class hierarchy from the bottom up. This
is implemented by casting `this` to a pointer to the most derived type, then
calling the member function. If the function is not defined in a given
extensible class, then the compiler will search the extensible class' parent.

We can tell the compiler what the most derived type is with a forward
declaration. To know the name of this class, we decide upfront what the name of
the most derived type will be and force the hierarchy to end at this name. By
ensuring that only the most derived type can be instantiated, we guaranty that
the cast is always to the type of the object referenced by `this`. This is why
only the concrete class may be instantiated.

For convenience, we define a special function in the base class called `self()`.
It simply performs a `static_cast` of the `this` pointer to a pointer to the
most derived type (which we forward declared) and returns the result. All calls
to member functions from within an extensible class must be prefixed with
`self()->`. Using the implicit `this` pointer **must be avoided** as it
circumvents the down cast
and can lead to strange and unpredictable behaviour.

> To enforce this rule, we use a clang-based linter, `OMRChecker`, that uses a
class annotation to verify that extensible classes are implemented correctly.

### Naming and Namespaces

As previously mentioned, we must know the name of the concrete class in a
hierarchy (most extended/derived type) when we declare the base class. Since we
consider all extension classes, extension specializations, and the concrete
class as essentially the same class, we give them all the same name and use
namespaces to distinguish them.

Each project has its own namespace to hold all of its class extensions. Nested
namespaces are used for the extension specializations provided by projects.
Concrete classes all go in their own, separate namespace, and this is shared
between all projects.

### Connectors

Sometimes, a class extension may only be needed in certain circumstances, such
as when compiling on a specific architecture. Other times, only one of a
collection of class extensions should be used. Connectors are an addition to
extensible classes that solve both of these problems by allowing classes
within the hierarchy to be conditionally specialized.

There are two parts to connectors. First, the include path of the compiler must
be carefully built to include the extension specialization if it is required.
Next, the extension specialization and its parent must signal to its child which
to extend from.

To better understand this section, an example will be used. Assume a project
contains the files `src/Class.hpp` and `src/amd64/Class.hpp`. The concrete class
uses the line `#include "Class.hpp"` to include its parent.

The first concept is easy to understand, but hard to implement. First, remember
that compilers search for headers in the order of the include paths given to
them. This means if a compiler is given an include path of `-Isrc/amd64 -Isrc`,
it will first search for a file in `src/amd64`, and only continue searching in
`src` if it did not find it.

In the example, the `#include` will include either the `Class.hpp` under `src`
or `src/amd64`, depending on the include path given to the compiler. Here, an
include path could be `-Isrc/amd64 -Isrc` to use the AMD64 extension
specialization, or `-Isrc` to not.

Second, the class that inherits from this needs to know to inherit from the
AMD64 version of the file or not. To solve this problem in a uniform way, we do
not inherit from the class directly but instead use what we call a "connector".
A connector is a `typedef` of the class extension/specialization. Instead of
being in the same namespace as its class, the connector is declared in the
project's namespace. Each specialization (including the _non-specialized_ class
extension) overrides the class extension's connector. `#define` guards are used
to ensure only one connector is ever defined for a given class extension. In
addition, to ensure the correct connector is selected by the compiler, its
definition must be placed before any `#include` statements.

Using connectors has the advantage of allowing a different extension
specialization to be selected by simply changing what files a compiler compiles,
without having to change the source code itself. Selecting what files are
compiled is handled by modifying the include path, the specifics of which is
explained later.

### Files and Directories

Another key aspect of extensible classes is how files and directories are
organized.

The simplest way to organize files is to create a separate directory for each
level of extension. That is, a directory for just the base classes, a directory
containing the first level of extensions, a directory containing the second,
etc. These directories will usually be implemented by different projects (one
directory per project that extends a class). We therefore call these
"project directories".

A project directory must, as a minimum, declare the class extensions it needs
and re-declare (or simply declare if the project is the base) the concrete
classes as inheriting from the new extensions. By convention, the name of a
file for a class extension should be the project's namespace followed by the
class name. Files for concrete classes should just be named after the class
name.

Project specializations must be put in sub-directories of a project directory.
These should not need to re-declare the concrete class, but may if needed.

If directories are used to organize the (potentially large number of) files in
a project, the organizational structure *must* be maintained across all projects
and specializations. So, if one project puts certain classes in a specific
directory, all other projects must put those same classes in a directory with
the same name. Also, to maintain compatibility across different compilers, the
name of these directories should be put as part of `#include` directives. This
is because certain compilers (e.g. `gcc`) always search the current directory
first, before looking in the include path.

### The Include Path

The last important aspect of extensible classes is how the include path
(i-path) is set. In order for the compiler to find the most extended version of
a class, the path to the most specialized/extended classes must be specified
first, followed by the more general ones, and so on until the path to the base
project.

Likewise, header files for extensible classes must be included from most
extended to most general. The only exception to this is the inclusion of the
header file for a particular class in the corresponding source file. In this
case, the file should be included first.

### The Linter and class annotations

When using extensible classes, there are many things to keep track of and many
things that can go wrong.  This is especially true when it comes to the use of
`self()`. Forgetting to use it can result in some very strange behaviour.

To help avoid this problem, we use a [clang
tool](http://clang.llvm.org/docs/ClangTools.html) (a linter) that detects and
reports these kinds of errors in the OMR project. When a class is annotated
with `__attribute__((annotate("OMR_Extensible"))`, the tool will
ensure that:

* `self()` is used were needed
* the base of extensible hierarchies are in the correct namespaces
* concrete classes are in the correct namespace, and
* the annotation is used in all classes of the hierarchy

For convenience, we also define the macro `OMR_EXTENSIBLE` to make the
annotation easier to read (and use) and to maintain compatibility with other
compilers.

Currently, however, the tool only searches for errors in the use of extensible
classes in the OMR project specifically.

## Extensible enums

Extensible enums are created by using an `#include` within an enum definition.
Using the same include path system as extensible classes, extensible enums
include the most specialized enum file. This file can optionally include
enumerator items and can `#include` more files, such as the file it is
overriding.

## Pros and cons of extensible classes

Pros:

* does not produce run-time overhead
* encourages good code base organization

Cons:

* large number of files
* no multiple inheritance nor multiple children
* easy to get wrong

## Comparison with other approaches

### Dynamic polymorphism

Dynamic (runtime) polymorphism, implemented in C++ through virtual member
functions, is an alternative to class extensions. There are benefits to using
this approach, such as better language and IDE support. For example, C++ allows
pure virtual functions and virtual methods can be called without the use of
`self()`.

However, dynamic polymorphism is less efficient, since each call to a virtual
member function requires dereferencing a pointer. Additionally, virtual member
functions cannot be inlined. Virtual member functions must also be marked as
`virtual` for dynamic polymorphism to be usable on that function.

Note that dynamic polymorphism does not provide an alternative to extension
specializations, however connectors can be adapted to work with dynamic
polymorphism.

### The CRTP

The Curiously Recurring Template Pattern (CRTP) uses templates to implement
static polymorphism. This has the benefit of supporting multiple hierarchies
from a common base class.

However, using CRTP requires base classes to make extensive use of templates.
In C++, this syntax is verbose and hard to get right. Additionally, member
function definitions must be in header files, which adds to compile times, in
some cases significantly.

Note that like dynamic polymorphism, CRTP does not provide an alternative to
connectors.

### Namespace Aliases

Namespace aliases are a proposed alternative to connectors. This method makes
namespaces accumulative, where if a class is not found in a namespace at
compile time, the compiler looks in its parent namespace.

This greatly reduces the boilerplate code required to implement connectors,
including the macros defined above a file's includes. Additionally, this
simplifies the question of what to inherit from.

This method however has a few downsides. If the wrong file is included when
using a class, some of the hierarchy would silently not be included. Similarly,
if a class extension doesn't include its direct parent but a different
ancestor, it can extend that class using its direct parent's namespace. This
can lead to some hard-to-debug issues.

More details about namespace aliases and the debate about their usage is
tracked in [issue #527 on Github](https://github.com/eclipse/omr/issues/527).
