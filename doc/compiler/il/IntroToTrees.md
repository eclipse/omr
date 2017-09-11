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

Testarossa's Intermediate Language: An Intro to Trees {#IntroToTrees}
=====================================================

The Testarossa compiler project began in 1998 as a clean-room Java JIT
implementation to address IBM's need for a high performing Java runtime for
embedded systems.  Over the past couple of decades, it has grown to become the
strategic compilation component for the many languages and server platforms IBM
supports.  Hence, the Testarossa effort has led to deep experience not only in
producing a production-quality compiler, but also in the engineering work
required to cope gracefully with the project's tremendous growth beyond its
original design parameters.  In this section, we give a brief overview of the
design of Testarossa's internal representation. 

Testarossa's internal representation uses DAGs (Aho, Sethi, Ullman 1986).  Each
node in the DAG represents an operation that produces at most one result value.
A node has an **opcode** describing the operation to be performed, and has zero or
more child nodes indicating operands.  Opcodes have **data types**, including
integer and floating-point types of various bit widths as well as other data
types such as complex numbers and various decimal number types that have proven
useful for the languages and platforms Testarossa supports.


## Side Effects

Some nodes may also have side effects, which we define as any ordering
constraint between nodes that is not embodied by the edges of the DAG.
Testarossa's IR manages side effects using symbol information.  Any node
producing or affected by a side-effect must have a symbol.  A doubly linked
list of elements called TreeTops (for historical reasons) winds though the
DAG and defines program order. The portion of the DAG reachable from a given
treetop is referred to as a "tree" (despite being a DAG), and the portion of
the DAG reachable from a given node is referred to as the "subtree" rooted at
that node (see an example of the trees representing an arithmetic expression in
the figure below). 

Testarossa has an important rule that a given tree can have at most one
side-effect, since otherwise the meaning of the program would be ill-defined.
Therefore, wherever correctness dictates that operations must occur in a
specific order beyond that dictated by the parent-child relationship (the DAG
edges), nodes must be anchored to treetops.


## Program Ordering

If a node is not directly connected ("anchored") to a treetop, then its position
within program order is loosely defined: a node's operation cannot be performed
until all its operands have been performed, but otherwise the ordering among
nodes is unspecified.  This is intentional, and provides a degree of freedom to
the optimizer and instruction selection to reorder operations in a limited
fashion without affecting program correctness. 

There is a concern, though, with DAG-based representations whose practical
importance cannot be overstated.  If a node is a child of multiple parent
nodes, it will naturally be performed at some point before its first parent is
performed.  However, if that first parent is modified by the optimizer so it is
no longer a parent of that child, then the child will naturally be evaluated
before the next (second) parent is performed.  This means the optimization will
have caused the child's location in program order to swing downward.  If the
intervening code has side effects that affect the child node, or vice-versa,
then the optimization will have incorrectly altered the program's operation.
The safe thing to do when "unhooking" a node from one of its parent nodes is to
anchor the child node at its original evaluation location, and the
infrastructure in any compiler using a DAG-based IR must offer facilities to
make this as easy and error-free as possible.  A side-effect-aware clean-up
pass called Dead Tree Elimination can run afterwards to eliminate any
unnecessary anchoring, freeing the other opts from worrying about the
performance impact of over-eagerly anchoring nodes.

## Control Flow 

Testarossa's DAGs are not permitted to span basic blocks; in effect, each basic
block (actually each "extended block", as will be explained shortly) contains
its own DAG.  This makes the basic block a first-class citizen of Testarossa
IR, and the control flow graph (CFG) is maintained all the way through the
compiler, in contrast to other IRs in which the control flow graph is deduced
from the program's instructions.  This has the drawback that every optimizer
transformation must continually repair the CFG to keep it correct, where other
IRs permit the compiler to discard and re-build the CFG on demand.

## Sharing

In a DAG representation, local common subexpression elimination (CSE) can be
represented by simply reusing an existing node rather than using a new
expression.  No stores and loads are necessary.  Testarossa's local CSE performs
local value numbering, eliminates the redundant expressions, and also trivially
performs local copy propagation during the same pass since it already has all
necessary information available.  The resulting DAG reflects all the information
that would ordinarily be provided by a local value numbering analysis and local
reaching-definition information in a natural and efficient manner that makes
subsequent analyses easy to write.  This is the primary benefit of the DAG
representation: it greatly simplifies many of the analyses performed by local
optimizations, turning relatively sophisticated analyses into simple
pattern-matching on the DAGs.



### Representation of `(a+b)*(a-b)`  

The following two figures show matching representations of the same expression, 
after some common sub-expression elimination has ocurred. 

    treetop----> istore a
                    |
                  imul--- isub---------+
                    |                  |
                  iadd                 |
                    |                  |
                    +-----> iload b <--+
                    |                  |
                    +-----> iload a <--+

This can be re-represented as textual representation. Note the node identifiers 
in the leftmost column, which show how `n4` and `n5` have been reused. 

    n1  istore <a>
    n2    imul
    n3      iadd
    n4        iload <b>
    n5        iload <c>
    n6      isub
    n4        ==>iload <b>
    n5        ==>iload <c>


## Basic Blocks

Thus, while it is always true that local optimizations are easier to write,
cheaper to run, and more powerful than global optimizations in practically any
IR, this effect is particularly pronounced in Testarossa's representation.  As a
result, Testarossa thrives on large basic blocks, so augment the power of local
optimizations, it offers a _block extension_ facility.  A basic block
extension pass runs midway through the optimization process after the global
optimizations are finished, and locates blocks whose only predecessor is the
previous block in program order.  Such blocks are flagged as "extensions" of the
previous block; a block plus all its extensions form a contiguous section of
code called an "extended block", with one entry point and multiple exits.  (Such
a structure is typically referred to as a _superblock_ in the
literature.) Many of the local optimizations are written in such a way as to
take advantage of extended blocks: most significantly, the DAGs are permitted to
cross block extension boundaries, extending the scope and benefit of local
analyses such as common subexpression elimination, constant propagation,
expression simplification, reassociation, idiom recognition, and instruction
selection.

