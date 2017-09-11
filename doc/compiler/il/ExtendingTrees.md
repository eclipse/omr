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

Extending Trees  {#ExtendingTrees}
===============

When adding a new feature to an IR, a question arises as to whether to add a
new opcode for the new feature, or to implement the feature using existing
opcodes. 

The most straightforward way to use existing opcodes is to insert a call to a
runtime routine that implements the new feature.  The actual runtime routine
may not, in fact, exist; rather, the compiler's instruction selection logic can
be made to recognize the routine and emit the desired instructions.  The call
node can carry symbol information that possesses the right aliasing properties
to make the optimizer treat the node in a suitably conservative manner, since
optimizers can do very little with calls to opaque runtime routines.  Generally
speaking, this option is most useful for cases in which the optimizer is
unlikely to find any opportunities to optimize the new feature anyway.

Another way to use existing opcodes is to craft a code sequence that implements
the new feature.  For example, a 64-bit divide operation could be implemented
by generating the long division sequence using existing 32-bit arithmetic in
the IR.  This technique makes the new feature amenable to the existing analyses
in the optimizer, but can be cumbersome, particularly if the feature involves
control flow.  After a few optimizations, the generated sequence may become
unrecognizable as a logical unit, which could make it harder to expose further
optimization opportunities specific to the new feature.

If neither of these techniques is suitable, it may be best to add a new opcode
for the new feature.  This involves inspecting existing code to reason about
how it will respond to the new opcode.

The following subsections discuss the intricacies that should be considered
when adding a new opcode.


Simplicity 
----------

An IL opcode is one of the most basic of building blocks in any compiler and so
each opcode should be simple to reason about.  Designing an opcode that behaves
in many different ways, particularly if it has different semantics on different
front ends or back ends, is particularly undesirable since it complicates not
only the design phase but also the testing phase of every feature involving
that opcode.  Unnecessary complexity at such a basic level is almost guaranteed
to make the codebase hard to understand, maintain, and extend over time since
it invariably permeates all aspects of the compiler.

In fact, this is one of the most important lessons we have learned in the
design of an IL: do not overload the semantics of an IL opcode.  Having several
simple opcodes instead of a single complex one runs the risk that they will not
enjoy the same level of success within optimizer, since a carelessly-written
optimization targeted at one opcode may not apply to all, but this is the
lesser of two evils when the alternative is to have a smaller set of IL opcodes
where each individual one is hard to reason about.

One example we encountered was a special variant of the left-shift
operation that Testarossa needed to support for one of the input languages.
This language required the left-shift to be sign-preserving. Our initial design
choice was to have to have different semantics depending on which input
language it arose from. This caused a large number of functional problems until
we created different opcodes to distinguish the different semantics of these
two operations.

In a DAG-based IR, the following characteristics are desirable in an IL opcode
from a simplicity viewpoint:
  
*  fixed number of children well defined semantics of what each of those
*  children mean; clarity on whether it has a symbol or not, and if it does,
*  what the symbol
   denotes from an aliasing or memory access perspective; 
*  a single return value; and a single side effect per top level tree. If the
*  opcode has multiple side effects then consider using a call or splitting
   into separate opcodes or nodes. 

So, is it ever desirable to favour a more complicated opcode instead of a
combination of simpler ones?  In some scenarios, a complex opcode can hide the
complexity associated with a particular operation from the common code
optimizer.  One such example is the Java newarray opcode; in theory it is
possible to create explicit control flow representing the different aspects of
this opcode, namely allocating the array by bumping a pointer if the allocation
can fit in the thread's local heap area, initializing the header fields and the
array elements, and going to some slow path to deal with various exceptional
conditions such as very large allocation sizes.  It would be possible to expose
all the control flow associated with such an operation, but that would break up
basic blocks which would unnecessarily complicate dataflow analyses, and would
disrupt local optimizations that would work better with a single DAG having all
the operations exposed as children in the IL subtree.

There are also some examples of operations that can be done much more
efficiently using special hardware instructions available on certain platforms,
such as CISC instructions to do memory copying or zeroing that can be far more
efficient than even the most efficient of loops when more than some threshold
number of bytes are to be written.

One example in our experience where having two simple IL opcodes instead of
overloading a single one would be beneficial is when representing a memory copy
in our IR.  There are essentially two variants of a memory copy in Java arising
from a) copying primitive values or b) copying references between the source
and the destination.  Since the JVM includes a garbage collector the reference
copy involves some extra bookkeeping that needs to be done when a generational
concurrent policy is in use; a primitive copy on the other is a straightforward
memory copy without any other overhead.  The semantics and indeed the inputs
are different enough between these two operations that we felt it would be a
cleaner design to separate them into different opcodes.  This was the result of
some re-engineering because we originally felt there were too many similarities
between the operations; with hindsight, we felt we could have avoided several
functional bugs that were a consequence of the single opcode having differing
semantics and number of children dependent on the kind of copying being done.
   
Another interesting issue stems from opcodes that could naturally be considered
to have multiple return values.  For example, on some platforms, the integer
divide operation also produces the remainder; or an integer add could produce
the sum along with a carry or overflow indicator.  The principle of simplicity
dictates that all operations produce just one result, leading to certain design
decisions that have proven their value over time.  Integer divide and remainder
operations are separate nodes, as are integer add, "integer add carry", and
"integer add overflow".  The optimizer then makes an effort to put such
operations adjacent to each other in the program order, and expects the back
end to do the necessary peephole optimization to combine the two adjacent nodes
into one operation.  This demonstrates a recurring theme throughout TR and
other practical optimizing compilers: the IR should make correctness easy and
leave performance as an optimization problem, rather than attempt to represent
optimizations in the IR and place a structural-correctness burden throughout
the compiler.  In the case of integer divide and remainder, for example, TR's
approach has the benefit that dealing with multiple results is cast as an
optimization problem, rather than an IR structuring issue that could lead to
correctness problems for any optimization or instruction selection code that is
not specifically designed to cope with multi-result nodes.

Likelyhood of being optimized by a common code optimizer
--------------------------------------------------------

A good first test to determine whether a new opcode should be created is
whether it is amenable to optimization.

For example, if the expressions including the new opcodes might become
invariant within loops, and, thus, become eligible for hoisting, or if simpler
transformations (e.g. constant folding, arithmetic simplifications) can trigger
a cascade of more powerful optimizations, the operation can be a good
candidate. 

Below we discuss two examples - one where a new opcode was created, and another
where it was not.
 
Converting calls to functions that implement common arithmetic calculations
into IR instructions may benefit multiple optimizations as well as more than
one front end and back end. `java.lang.Integer` and `java.lang.Long` include
static methods for computing the number of set bits, highest/lowest one bit,
and the number of leading/trailing zeros.  Methods with the exact same
semantics are also provided by most C/C++ compilers.  Introducing IL opcodes to
represent these methods in IR enables the optimizer to perform traditional
constant folding in case the arguments are constants.  Optimizations handling
numeric ranges can further constrain the operands' ranges in a comparison and
make it amenable to constant folding. The IR nodes including the new opcodes
can also be hoisted out of loops if proven to be loop invariant.

Consider an example of a comparison: 

     ...
     if (bitCount(i)*2 + 3 > 32) { //deadcode
        j *= 2;
        k += j;
     }
     ...


Let us assume that `i` provably falls in [0-16].

`Integer.bitCount` returns a number in [0-64] given an arbitrary input;
however, `i` further constrains the range to [0-4], making it possible for us
to fold the entire inequality into `false`. 

Similarly, adding instructions to represent functions for computing max/min
allows optimizer to fold away an instruction into a constant, or at least to
reduce the number of arguments by comparing the constant operands to each
other.  The computations including the new instructions can also be hoisted out
of a loop if proven to be invariant. 

Another example that might at first seem a good fit for creating an opcode
would be the 'compare and swap' primitive.  This primitive is used in many
languages in the implementation of mutexes and other synchronization
primitives.  As well, many high level languages also make this primitive
directly accessible to the programmer.  For example, Java's concurrent
libraries include primitives such as `AtomicInteger.compareAndSet`.  A call to
this method takes two integer parameters - an expected value and a new value.
Should the current value be equal to the expected value, the `AtomicInteger`
object is updated with the new value, and a boolean value is returned to
indicate success.  This sequence of operations must be done atomically usually
by executing the relevant atomic hardware instruction available on the
architecture.  It could be argued that creating an opcode for this operation
would be a good thing - not only do other languages have similar primitives,
but underlying architectures may have differing implementations of atomic
instructions, so abstracting them to a single opcode would make sense.


However, in this particular case, we decided not to because there is very
little opportunity to optimize a `compareAndSwap` operation.  At a high level,
atomic operations are generally used to update shared variables between
threads.  In the case of Java's AtomicInteger, the field being written has
implicit volatile semantics as defined in the JVM specification Java's memory
model has defined very specific behaviour in regards to volatile variables.  In
particular, when a write occurs, it, along with any other non-volatile writes
to the same thread become visible to all other threads.  In practice, this is
done with a memory fence operation on a CPU.  Since the volatile read or write
establishes ordering constraints on memory accesses, it becomes very difficult
to perform code motion or eliminate one of these accesses.  To do so would
require knowledge of global state, which most compilers do not have, and in the
case of dynamic runtimes, may not even possible as classes may be loaded or
unloaded at any point.  As such, the compare and swap opcode would be ignored
by the optimizer.

Liklihood of being used across different languages. 
---------------------------------------------------

The value of adding a new opcode for a new feature depends on how widely it
will be used, and on how much benefit it offers when it is used.  One measure
of widespread adoption is the number of front ends and back ends expected to
use the opcode.  For _M_ front ends and _N_ back ends using the opcode, the
benefit is something like _M x N_, while the cost is proportional to the amount
of disruption required in the optimizer to provide support.  All else being
equal, a feature used by multiple front ends and multiple back ends is much
more likely to be worth the engineering effort and disruption to the optimizer.

Measuring the benefit of the new opcode is harder.  The benefit depends to some
extent on the nature of the optimization opportunities it exposes, and this
effect could be quite indirect.  For example, if the new feature would require
a control flow merge point, that breaks basic blocks, which reduces the scope
of local optimizations and significantly impairs the optimizer.  On the other
end of the spectrum, if the new feature is implemented as a call to an opaque
runtime routine, this solves the problem of breaking blocks by hiding the
control flow, but it limits the optimizer to the subset of optimizations that
it can perform on calls to opaque runtime routines, which is a tiny (almost
empty) subset.

Ironically, if the optimizer needs no modification at all to support the new
opcode, it is possible that this indicates the new opcodes should not be added:
it may be that the optimizer has no opportunity to take advantage of the new
opcode, and so a call to a runtime routine would be equally suitable.


Uniqueness 
----------

Every IL opcode should perform a significantly enough different function from
existing opcodes. This function needs to be clearly defined. Let's take
`iadd` (int add) and `fadd` (float add) Java bytecodes as example.  A floating
point addition does a siginificanly different function on the input set of
32-bit than an integer add, so it is clear to see those as distinct opcodes. It
is slightly less clear if a 1-byte integer add is sufficiently different from a
2-byte or 4-byte integer add. These operations work sufficiently similarly,
just on different sized operands. See also the next section on _Represnting
Types_ for a more detailed analysis of how types play a role in the design of
an IL.

If there are several opcodes which perform mostly the same function, it becomes
harder for the optimizer to pattern match and optimize. All the places in the
optimizer that deal with one variant of the operation have to be made aware of
the second variant that performs mostly the same function. But since these two
opcodes aren't exactly the same, the optimizations have to be really careful to
reason about the differences.

One extreme example is this is that we used to have signed and unsigned variants
of integer add opcodes. Many optimizations were designed to deal with only one
variant, so they left the other un-optimized. Upon reflection it is clear that
the operation performed by both of these operations is exactly the same.

Representing Types
==================

An optimizer typically has to deal with operands with at least a few different
primitive types (such as integer, short, address, etc.)

Some language front ends might require the optimizing compiler to distinguish
these types clearly for functional reasons. e.g. low level languages like C
expose signed and unsigned bytes, shorts, integers, etc. as distinct types in
the language. Even in purely object-oriented languages where everything is an
object, an optimizing compiler typically still needs to support auto-unboxing
for performance reasons. Taking JavaScript as an even more extreme example,
where numbers are supposed to be IEEE floating point values, an optimizing
compiler might still want to use integer arithmetic wherever possible for
performance reasons.

One decision that an IL designer has to make is how tightly coupled opcodes
should be with types. As an example, should a byte add or a floating-point add
operation be a distinct opcode from an integer-add operation? Should unsigned
operations be distinguished from signed operations? How much overlap should
address types have with integer types?  Are decimal floating point operations
distinct operations from IEEE floating point?

Since Testarossa IL started out as an IL for Java, similar to Java bytecode, the
opcodes are typed based on operand size. For example, we started out by having
distinct opcodes for 4-byte integer shift left (`ishl`) vs. 8-byte
integer shift left (`lshl`). This convention applied to most arithmetic,
logical, comparison, load and store, and conversion opcodes. Many of our opcodes
were verbatim copies of corresponding Java bytecode opcodes. While most
operations in bytecode are signed, there are a few 'unsigned' operations such as
`iushr`, which represents an unsigned right shift operation. Eventually
our IL was expanded to support non-Java languages as well, and at that time new
opcodes were added to support unsigned integer types. We continued the
conventions and added unsigned opcodes (e.g. `iuadd` - add of two
unsigned integers).

As a result our IL ended up with strict typing and a large explosion in the
different variants of each type of operation. For example, here is a list of
all the 'shift right' opcodes we ended up with: `bshr, bushr, sshr, sushr,
ishr, iushr, lshr, lushr`. Here the `shr` suffix represents the operation
(shift right) and the remaining characters encode the types. `b, s, i,` and `l`
correspond to operand size `byte, short, int,` and `long` respectively. The `u`
character encodes the unsigned version of the operation.

This explosion in the number of codes is not necessarily a good design point.
There is a trade-off involved here. On one hand type-strict opcodes lead to a
design that is simple to understand and faster to write a code-generator for. On
the other hand, it is harder to write higher level optimizations (such as loop
optimizations) more generically. 

We ended up creating a more abstract interfaces ("families of opcodes") on top
of our IL in order for the optimizations to work with operands and operations in
a more generic fashion.  The existence of this interface is evidence of the
disconnect between the type-strictness and how higher level optimizations would
like to work with IL.

One of the key realizations is that there are two attributes of an integer type
that have different effect on an IL: 1) Operand size, and 2) signedness. These
are best dealt with differently rather than under a singular concept of 'type'.
In our experience, signedness is an attribute of opcode (where sign and unsigned
operations are truly unique and distinct). On the other hand, the operand size
is more a property of the operand rather than the opcode.

Signedness
----------

While many people consider the sign to be a property of the type, in our
experience it is actually related to the operation than to the operand. From a
representation perspective, a signed byte looks exactly the same (8-bits) as an
unsigned one (8-bits). The difference comes in how these bit patterns are to be
interpreted and consumed (by "operations"). Take the expression `(value >> 8)`
from C as an example where `value` is declared as an `unsigned int`. We find
that it is better to think of this expression as "an unsigned right shift of a
32-bit operand" rather than "a right shift of a 32-bit unsigned operand". An
unsigned-shift-right performs a different function on the bits than a
signed-shift-right, and neither operation is affected by whether the 32-bits
passed-in were nominally signed or unsigned.

Note that this is very different from how languages define types and operators.
From a programmer's perspective it is much easier to write a `<` (less
than comparison) symbol and have it behave differently based on the type of the
operand instead of having a different operators for signed and unsigned
comparison.

The benefit of separating the concept of signedness from type is that a lot of
unnecessary complexity in the IL immediately becomes evident. Compare the following
trees for the expression `((x+1)>>8) > 5)` where the right shift was intended
to be an unsigned shift but the comparison is supposed to be signed comparison

     ificmpgt -> goto block 5
       iu2i
         iushr
           i2iu
             iadd
               iload <x>
               iconst 1
           iconst 8
       iconst 5

with 

     ificmpgt -> goto block 5
       iushr
         iadd
           iload <x>
           iconst 1
         iconst 8
       iconst 5

The unnecessarily complex trees arise from a type-strict notion of operands.
The problem here was the requirement that `iushr` requires an unsigned integer
and produces an unsigned integer. This resulted in redundant conversions to and
from unsigned integers.  By removing the notion that integers can be signed or
unsigned we simplify to the notion there are certain operations that performed
an unsigned (logical) function. Operands don't have a signedness property.

We no longer have the concept of an unsigned (or signed) integer operand in our
IL. We have distinct opcodes for signed and unsigned operations when the
function is actually different between the two versions. For example, we have
distinct integer compare-for-order opcodes for signed and unsigned. We only have
a single integer compare-for-equality opcode.


Operand Size
------------

Let us take the `badd` and `iadd` as examples. The former takes two 8-bit
operands and produces an 8-bit addition of those operands. The latter does the
same, but with 32-bit operands. Distinguishing these operations as distinct
makes it easier to write a code-generator for these operations or to do
peephole optimizations. For example, depending on the platform, it may be very
easy to map these to specific operand-sized registers and instructions.  In our
experience, however, having distinct opcodes for each operand size makes it
harder to write higher level optimizations such as loop optimizations. From an
optimizability perspective, the same types of optimizations can be performed on
a `badd` as a `iadd`, for example.

While we are not at this point in our IL yet, our recommendation for higher
level optimizers is to omit the operand size from the opcodes. Instead it is
better to have a singular integral `add` opcode which does the appropriate
sized add based on the operands that it is provided. The operand size
information is ultimately derived from the symbols or constants that constitute
an expression are not an inherent property of the operation itself.  This makes
the optimizer much simpler as analyses can be writen in a much more
generic fashion.

The trade-off of _simplicity_ and _uniqueness_ against _optimizability_ becomes
apparent here. It is indeed simpler to have `badd` and `iadd` as distinct
opcodes. A generic integral `add` opcode requires inspection of the operands to
truly reveal its operation. The trade-off should be made based on whether or
not the optimizer is expected to perform higher level optimizations that are
best written in a type-independent fashion.

We do recommend distinguishing a floating point adds from an integral add, since
we do not perceive that the same types of optimizations would apply to both.
Here the _Simplicity_ and _Uniquenes_} principles prevails as we don't
have a strong _Optimizability_ incentive.


