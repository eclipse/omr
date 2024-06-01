# Data-Flow Engine

To perform global optimizations on a compilation unit one of the primary utilities the OMR optimizer will make use of is the data-flow engine, which is the backbone for a variety of data-flow analyses that we support. The data-flow engine in OMR is not front and center to most developers. It is a piece of technology that has been battle tested for years and developers can expect it to _just work_. This document will walk us through some of the key concepts of the data-flow engine which will empower you, as a developer, to understand the inner workings of existing analyses and be able to write your own data-flow analyses with minimal effort.

Throughout this document we will be referencing the implementations of _reaching definitions_ and _live variable analysis_ in OMR as the two canonical examples of global data-flow analyses typically found in literature.

## Virtual Hierarchy

The entire data-flow engine is based on virtual inheritance, in which each class builds upon its parent to provide additional functionality and the leaf class is typically the concrete data-flow analysis one may find in literature. Tracing a path through the virtual hierarchy will build different analyses based off of what information (sets) we wish to be flown around the program, in which manner (forward & backward), and how we wish information to be aggregated (union & intersection) as the data-flow engine traverses the CFG.

To aid in understanding how this virtual hierarchy works let's consider the following simple program:

```
int y = 5;
int z = 0;

for (int x = 0; x < 10; ++x) {
    z = z + y;
}

print(z);
```

It is trivial for a human to deduce that the variable `y` is not modified within the loop, and that there only exists one possible definition of `y` which reaches the use inside the loop. Therefore we should be able to propagate the right hand side of the definition of `y` to the use inside the loop, meaning we can simplify the program as:

```
int z = 0;

for (int x = 0; x < 10; ++x) {
    z = z + 5;
}

print(z);
```

For a compiler to be able to deduce this on arbitrary programs it must use a data-flow analysis called _reaching definitions_ which for a particular _use_ of a variable at a particular point in the program gives the list of all possible definitions (_def_) of that variable that can reach the particular use.

[ReachingDefinitions.cpp](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/ReachingDefinitions.cpp) implements the reaching definitions data-flow analysis. We will use this as an example to explain the virtual hierarchy of classes which drive data-flow analyses.

### [`TR_DataFlowAnalysis`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L98-L223)

The root of the hierarchy that all data-flow analyses start at is `TR_DataFlowAnalysis`. This class contains mostly boilerplate code that aids in memory allocation, access to commonly used data structures, aliasing questions, tracing, and debugging information. This class also provides pure virtual functions which must be implemented by the derived classes. Developers writing new data-flow analyses will typically not be inheriting this class.

### [`TR_BasicDFSetAnalysis<T>`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L225-L359)

The `TR_BasicDFSetAnalysis<T>` inherits from `TR_DataFlowAnalysis` and is a templated class representing data-flow analyses which use containers (sets) of bits which are flown around the program during the analysis. The set is represented by the template parameter `T` and it can be either a `TR_BitVector` or bit set (for example `TR_SingleBitContainer`).

There is only one other class which inherits from `TR_DataFlowAnalysis` and that does not user bit containers as the underlying data structure; it is the [exception check motion](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/PartialRedundancy.hpp#L150-L276) data-flow analysis found in the Partial Redundancy optimization. It uses linked lists as the underlying data structure flown around during the data-flow analysis. It is the only non-bit container based data-flow analysis in OMR at the moment.

#### Counting and initialization

The `TR_BasicDFSetAnalysis<T>` contains many virtual functions which subclasses will override. One such example is the [`getNumberOfBits`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L262) API which subclasses will use to initialize the sizes of the sets which will be used in the data-flow equations. It is up to the subclasses to decide how _number of bits_ are counted. For example some times the number of bits depends on the number of symbol references, sometimes the number of certain type of nodes if we are counting loads or stores for example. In case of reaching definitions [`TR_ReachingDefinitions::getNumberOfBits`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/ReachingDefinitions.cpp#L92-L95) returns the total number of definitions seen in the compilation unit.

Other pure virtual functions include initialization functions [`initalizeInfo`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L264-L265) and [`inverseInitializeInfo`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L264-L265) which initialize the template container based on the number of bits. These APIs are meant to be implemented by subclasses depending on the type of confluence operation performed which is discussed below.

#### Union & intersection confluence operators

In literature, data-flow analyses are often expressed in terms of data-flow equations which give us a blueprint for how to compose information as we traverse the CFG. These operations are called confluence operators and when dealing with bit sets they often map to set operations such as set union, intersection, and difference.

Intuitively set _union_ confluence operations aggregate data as we traverse the CFG. The bit sets start off with all bits cleared and we add information as the data-flow engine traverses the CFG. Reaching definitions is an example of a set union data-flow analysis, since we want to discover the minimal set of definitions which may reach a particular use. On the other hand, set _intersection_ confluence operations refine data as we traverse the CFG. The bit sets start off with all bits set and we remove information as the data-flow engine traverses the CFG. [Delaydness](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/Delayedness.cpp#L39-L50) is an example of a set intersection data-flow analysis found in PRE.

In OMR the confluence operation is implemented via a function call to APIs defined within the `TR_BasicDFSetAnalysis<T>` class; the [`compose`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L266-L267) and [`inverseCompose`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L266-L267) APIs. These APIs are meant to be overriden in subclasses and may have different implementations depending on what our confluence operator is (union vs. intersection). The `inverseCompose` API is, as the name suggests, the inverse operation of the `compose`. So if union is the `compose` confluence operator, then intersection would be the `inverseCompose` operation.

#### Support for _gen_ and _kill_ sets

In literature, data-flow equations are typically expressed in terms of _in_, _out_, _gen_, and _kill_ sets. The _in_ and _out_ sets represent the information flowing into a particular program fragment or out, respectively. The _gen_ and _kill_ sets represent the information generated and killed by a particular program fragment, respectively. Depending on the direction the data-flow analysis is operating (more on this later) the _in_ or _out_ sets are typically expressed in terms of the _gen_ and _kill_ sets.

For reaching definitions the _gen_ set would represent the set of variable definitions that appear within a program fragment reach the end of the program fragment. The _kill_ set would represent the set of all definitions that never reach the end of the program fragment.

The _program fragment_ in OMR can be as small as an individual treetop or as large as a _region_ which could be a collection of basic blocks. The notion of whether a data-flow analysis supports _gen_ and _kill_ sets is expressed via the [`supportsGenAndKillSets`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L260) API which is meant to be overridden by the subclasses. For analyses which return true for this query, the data-flow engine will be setup to operate on _gen_ and _kill_ sets which are represented by the template parameter and operate on a larger program fragment known as a _region_ (see [Operation on Structures and Regions](#Operation-on-Structures-and-Regions) section). This is mostly an internal caching mechanism to improve compile time performance of computationally expensive data-flow analyses.

What is the alternative? Instead of using _gen_ and _kill_ sets we would walk the treetops and the descending nodes every time. Since data-flow is iterative it can cause us to visit the same block several times while traversing the CFG in order to converge. If we are in a deeply nested loop we may visit the same block many times, so rather than visiting the nodes each and every time we analyze the block we would cache that information in the _gen_ and _kill_ sets for that block and use that to generate the _out_ set for the block.

#### Container node number pair

A space saving optimization found within the `TR_BasicDFSetAnalysis<T>` class is the notion of [`TR_ContainerNodeNumberPair`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L286-L296) which encapsulates the notion of multiple _out_ sets for a particular block. We need to represent the solutions to the data-flow equations the successors of a particular block. There are usually only a handful of successors, so rather than allocating a large array of arity equal to the number of blocks to hold the solution to the data-flow equations, we simply use this pair data structure to represent a linked list of solutions to the successors.

### [`TR_ForwardDFSetAnalysis<T>`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L362-L411) and [`TR_BackwardDFSetAnalysis<T>`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L506-L553)

Moving down into the hierarchy the `TR_ForwardDFSetAnalysis<T>` and `TR_BackwardDFSetAnalysis<T>` classes inherit from `TR_BasicDFSetAnalysis<T>` and represent the orientation in which the traversal of the CFG is to be performed by the data-flow engine. These two classes implement the core of the engine which determines which program fragment will be examined next and supplies additional virtual functions which can be overridden by the subclasses.

A fairly concise example of what type of functionality these classes provide can be found in the [`analyzeBlockStructure`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/BitVectorAnalysis.cpp#L1385-L1413) implementation. This API is called by the `TR_Structure` object to perform data-flow analysis for a structure containing a single block. We can clearly see how in the forward data-flow analysis case we query the successors of the block and we use the node number container pairs to look for the precomputed _out_ sets which we aggregate.

### [`TR_UnionDFSetAnalysis<T>`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L445-L462) and [`TR_IntersectionDFSetAnalysis<T>`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L413-L443)

There are actually four classes within this category:

- `TR_UnionDFSetAnalysis<T>` and `TR_IntersectionDFSetAnalysis<T>`
- `TR_BackwardUnionDFSetAnalysis<T>` and `TR_BackwardIntersectionDFSetAnalysis<T>`

The former two represent forward analysis while the latter two represent backward analyses. These classes implement the final step in the current data-flow engine virtual hierarchy, right before leaf classes are typically implemented. These classes represent the two most common confluence operations; union and intersection. Given what we know about confluence operators from the previous sections and how they map to function calls in OMR, these classes provide concrete implementations of the `compose` and `inverseCompose` APIs.

The implementation of these classes are quite trivial as the set union and intersection operations are implemented within the container classes. Never the less they are an important piece of the virtual hierarchy before we reach the leaf classes.

Reaching definitions is an example of a forward union set data-flow analysis while [liveness](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L840-L871) is an example of a backward union set data-flow analysis.

### Leaf Classes

The final step in the virtual hierarchy are the leaf classes, which typically map to textbook examples of data-flow analyses. [`TR_ReachingDefinitions`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L480-L504) and [`TR_Liveness`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/DataFlowAnalysis.hpp#L840-L871) are two examples of leaf classes. By the time we reach the leaf classes, many of the virtual functions have already been implemented for us by the super classes. You may note that there isn't a lot of code in these leaf classes. This is a good thing, and it means the APIs in the virtual hierarchy have implemented most of the functionality for us, and we as developers are only left with modeling some specifics about our particular data-flow analysis, such as initialization and counting.

## Operation on Structures and Regions

As previously discussed, _gen_ and _kill_ sets are typically thought of in terms of basic blocks, however in the OMR data-flow engine they are thought of in terms of [`TR_Structure`](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/Structure.hpp#L58-L257). This design decision was made to give us the freedom to be able to generate _gen_ and _kill_ sets not just on a black basis but in terms of larger program fragments which improves performance of the OMR compiler when used in JIT compilation environments.

A `TR_Structure` is composed one or more:

- Basic blocks or a collection of basic blocks (`TR_Region`)
- Natural loops
    - A well behaved loop for example which has one entry which dominates all the other regions in the loop
- Acyclic structure
    - Collection of structures where there is no cycle
- Improper region
    - Collection of structures where there _may_ be a cycle

Computing the structure is a prerequisite to running the data-flow engine. Structures allow us to work on larger program fragments ("chunks") which improves compile time performance. Structures also have a caching mechanism built in for data-flow analysis in which we can keep track of the _in_ and _out_ sets while running a data-flow analysis. If we for example find the same _in_ set we can return the computed cached _out_ set since the program is not altered during data-flow analysis.

When it comes to _gen_ and _kill_ sets and their relations to structures, there are two notions to consider:

1. Using _gen_ and _kill_ sets for each block structure, which means that treetops do not need to be walked as often during the actual dataflow analysis (as described earlier in this document)
2. Using _gen_ and _kill_ sets for entire region structures at a time, which means that the structure sub-nodes do not need to be traversed within the region structure during the actual dataflow analysis

These two notions of using _gen_ and _kill_ sets are controlled by different queries in the dataflow analysis infrastructure.

Some data-flow analyses may get disinterested in using _gen_ and _kill_ sets for structures if improper regions are encountered. This is true for the backwards analyses at the moment. The presence of an improper region only affects the latter notion (2) of using _gen_ and _kill_ sets on region structures. In other words, the presence of an improper region does not affect whether or not we will not walk the treetops, and that only depends on whether _gen_ and _kill_ sets are supported on even block structures or not.

In general using structures as our program fragments enables us to save roughly 10% compile time performance during data-flow analyses.

## [OMRDataFlowAnalysis.enum](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/OMRDataFlowAnalysis.enum)

The (mostly up-to-date) list of all data-flow analyses classes supported in OMR are listed in this file. This file can be used as an extension point to add new data-flow analysis in downstream projects consuming OMR.

### Local Transparency

Local Transparency answers the question: Can this expression be moved pass this block? After building up the correspondence of which
expression depends on which sym ref, Local Transparency checks which sym refs are being written in the block, and then it will know
which expression is transparent or can be moved across the block. If the value of this expression does not change in this block, this
expression is considered locally transparent to the block. If the value of the expression is changed in the block, the expression is
not locally transparent to the block. The expression cannot be moved across this block.

For example, there is a computation of `x+y`. Where `x+y` is computed and where `x+y` should be computed (the optimal spot) can be many
blocks apart. Local Transparency determines if `x+y` can flow through all the blocks to the optimal spot where it should be computed.
If the block does not change `x` or `y`, `x+y` can be moved pass this block. If `x+y` is computed earlier, and then it is computed again,
and `x` and `y` have not been changed in between, the two computations can be commoned by Partial Redundancy Elimination (PRE).
The value of `x+y` can be stored in a temporary which can be used in later blocks instead of computing `x+y` again.

[TR_LocalTransparency](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/LocalAnalysis.hpp#L230)
runs before [TR_LocalAnticipatability](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/LocalAnalysis.hpp#L286):
- `TR_LocalTransparency::_transparencyInfo[symRefNum]`: A bit vector indicates which expressions (identified by local index) will not be
affected by the sym ref if the sym ref is changed in the block
- `TR_LocalTransparency::_info[blockNum]._analysisInfo`: A bit vector indicates whether or not the expression (identified by local index)
is transparent to the block

### Local Anticipatability

Local Transparency does not look at whether or not an expression is evaluated in a certain block. It looks at an expression regardless
of where it is computed. Local Anticipatablility looks at the expressions that are computed in the block. If an expression is locally
anticipatable in a block, the expression is computed in the block and can be moved out of the block without changing the value of the
expression. For example, if `x+y` is computed in `block_10`, the expression `x+y` can be a candidate of being locally anticipatable in
`block_10`. If `x+y` is not computed in `block_10`, the expression `x+y` will not be in the Local Anticipatablility set.

[TR_LocalAnticipatability](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/LocalAnalysis.hpp#L286)
runs after [TR_LocalTransparency](https://github.com/eclipse/omr/blob/f2bc4f8f6eb09f6cc8fc4ba48717de4880b970e3/compiler/optimizer/LocalAnalysis.hpp#L230)
- `TR_LocalAnticipatability::_info[blockNum]._analysisInfo`: A bit vector indicates whether or not the value of an expression
(identified by local index) would be changed if it were computed at the very start of the block (upward anticipatable notion)
- `TR_LocalAnticipatability::_info[blockNum]._downwardExposedAnalysisInfo`: A bit vector indicates whether or not the value of an
expression (identified by local index) would be changed if it were computed at the very end of the block
- `TR_LocalAnticipatability::_info[blockNum]._downwardExposedStoreAnalysisInfo`: A bit vector indicates whether or not the value of a
store expression (identified by local index) would be changed if it were computed at the very end of the block

Both the upward and downward exposed notions of this analysis are valid for their own use cases. In an ideal world, we would be doing
both kinds of analyses but this takes up more compile time than we can likely afford in a JIT compiler (especially as the following
dataflow analyses would also need to solve for both notions). Therefore, we currently only use the downward exposed notion for our
implementation since we feel that this is relevant for more of the common and expected cases to be optimized.

### Global Anticipatability

Global Anticipatablility looks at if an expression can flow through the CFG. If an expression is globally anticipatable at a certain point
1. It is evaluated (i.e. encountered or computed) on all paths following that point, meaning adding the expression at that point
will not make anything worse
2. If it is evaluated at that point, the value of the expression would be the same as the next time the expression is encountered
on all paths following that point

If an expression is globally anticipatable in all the successors of a block and the expression is locally transparent in that block,
the expression is global anticipatable in that block.

### Earliestness

Earliestnees is a subset of globally anticipatable points where expressions can be computed as early as possible.

### Delayedness And Latestness
Delayedness and Latestness give a subset of globally anticipatable points where expressions can be computed as late as possible.

### Related Resource
1. S. Muchnick. Advanced Compiler Design and Implementation. Chapter 13 Redundancy Elimination
2. V. Sundaresan, M. Stoodley, P. Ramarao. Removing Redundancy Via Exception Check Motion
3. J. Knoop, O. Ruthing, B. Steffen. Lazy Code Motion
