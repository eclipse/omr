# Value Propagation

Value propagation (VP) is one of the most powerful optimizations in the OMR compiler. It does several optimizations and analyses that one may typically find in literature, such as type propagation, constant propagation, range propagation, etc. The VP optimization combines these sorts of analyses all into one.

Every optimization in OMR can ask for certain analyses to be done ahead of time before the optimization runs. Value numbering, among other analyses, is a [prerequisite to run VP](https://github.com/eclipse/omr/blob/1d3980b61ba7ef43d0f113cf9f2ad88f79d885ab/compiler/optimizer/OMROptimizationManager.cpp#L99-L101) since the entire framework relies on having value numbers available for all nodes.

## Value Numbering

Value propagation, as the name suggests, is based on values and in particular the notion of value numbers. Value numbering is a well known analysis in compiler theory. At a ten-thousand foot level, what value numbering does is it picks nodes which are provably known to have the same value and assigns them the same value number.

Note that two nodes may represent the same value need not always have the same value number. However, if two nodes represent different values then their value numbers _must_ also be different.

Value numbering encompasses notions like definitions and uses. For example, if we take the value `4` and store it to a temporary, say `y = 4`, and load this temporary value several blocks later. The value numbering analysis is capable of realizing that it is the same value flowing through all those areas and it will give the same value number to the right hand side of the store and to the left hand side of the store, in addition to all loads that are fed uniquely by that store.

This is the simplest example when value numbers kind of flow through and are the same, and give us a sense that we are talking about the same value which has the same underling properties.

On the contrary, to give us an intuitive sense for a case when we won't have the same value number is to examine what happens during control flow:

```
if (...) {
    x = 1;
} else {
    x = 2;
}

y = x;
```

In this case the value number of the right hand side of the assignment `y = x` will be different than the value number of the left hand side of the stores to `x` in the if-then-else blocks, because at compile time we may not be able to determine which path we would take, so all three nodes will have different value numbers.

Another important property of value numbers that we should understand is that the notion is beyond just syntax. We can have two expressions such as `x + 5` and `y + 5` where `x` and `y` are known to be equal over a particular range, and thus the two addition expressions will also have the same value number. Thus value numbers are not all just trivial use-def chains and parent-child sort of relations. We can have different nodes (expressions) that don't particularly look alike have the same value number.

This is a very high level summary of what value numbering is. There are excellent resources in literature or online to learn more about value numbering.

## Constraints and Handlers

[Value propagation constraints](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/VPConstraint.hpp) are the fundamental classes which are flowed around by the value propagation analysis. Their purpose is the represent constraints determined by the analysis on _value numbers_. If for example we had an expression such as `x = 5` then we would create a constraint which represents the constant value of `5` and we would associate this constraint with the value numbers for the nodes representing the left hand side (`istore`) and right hand side (`iconst`) nodes. Remember that these two nodes would have the same value number because we can prove that they are equal. The VP handlers consume these relations between value numbers and constraints to optimize the program.

[Value propagation handlers](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/VPHandlers.cpp) are functions which operate on the set of constraints associated with the node's children and produce constraints for the parent node. There are many handler functions associated with many opcodes. The [value propagation table](https://github.com/eclipse/omr/blob/ad822fbd7f17f1aa1fddafa355cb18fac4247a46/compiler/optimizer/ValuePropagationTable.hpp) describes which function to execute for each particular opcode. Some opcodes are not interesting and all they do is constrain their children, while others perform an interesting operation.

The best way to understand what a VP handler does is to look at an example. Let's consider the case for a simple integer multiplication which is handled by the [`constrainImul`](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/VPHandlers.cpp#L5976-L6068) function. This VP handler takes the two constraints of the children of the multiplication and analyzes them. If the constraints represent constants, it multiplies the values of the children constraints and creates a new constant constraint for the parent node. If the constraints represent ranges of values the handler calculates what the maximum and minimum ranges would be of the multiplication and creates a new range constraint for the parent node.

Note that if you examine how the constraints of the children are obtained, you will see that they are extracted using the node's value number. Going back to our previous example of creating a constant constraint for an `x = 5` operation it is fairly straightforward to see that for our multiplication example if one of the children happened to have the same value number as `x`, then it means we can replace the child with a constant load of `5`. It is very important to understand that the child of the multiply can be arbitrarily complicated, yet as long as it has the same value number as `x` we can reduce it to a constant. This is the essence of the value propagation optimization.

## Intersect and Merge Operations

Every constraint class will have an `intersect` and `merge` APIs which can be overridden by subclasses. Intersection and merging of constraints only happens at control flow points in the program and they determine how information about value numbers and their constraints is propagated through the rest of the analysis.

An `intersect` of two constraints will make the constraint more refined. For example consider the following program:

```
if (x < 10) {
    if (x < 5) {
        ...
    }
}
```

Let's assume we knew nothing about the value number of `x` prior to the first `if` statement, i.e. the value number of `x` had a range constraint of `[INT_MIN, INT_MAX]`. The VP analysis would call on the handler which constrains the first `if` expression. At this point we need to `intersect` the existing constraint with the new information that we have learned from the `if` statement, i.e. we would refine that within the `if` block the value number of `x` will have a range constraint of `[INT_MIN, 9]`. Next the VP analysis would constrain the second `if` expression. At this point we can further `inersect` (refine) the value number of `x` as having a range constraint of `[INT_MIN, 4]` within the second `if` block.

In summary, intersecting constraints only makes the information known at the time better, or at the same level as before. This means we learn progressively as the analysis flows through the program and are able to make better decisions.

A `merge` of two constraints will weaken the constraint. For example consider the following program:

```
if (...) {
    if (x != 10) 
        return;
    ...
} else {
    if (x != 5)
       return;
    ...
}

...
```

After analyzing both the outer and inner `if` and the `else` blocks the analysis needs to continue with the merge point of the control flow. At that point we must `merge` the set of constraints we have determined within the outer `if` and the `else` blocks. In the above example the value number of `x` following the merge point would have a constraint which merges the constraints from within the previous blocks, i.e. a discrete range constraint specifying that the value number of `x` can be in ranges `[10, 10]` or `[5, 5]`, since the only way to reach the merge point is if we did not execute the `return` statement.

Note that we typically don't represent discrete values, but rather ranges of discrete values, which are easier to merge with one another.

In summary, merging constraints only makes the information known at the time worse, or at the same level as before.

## Examples of Constraints

### Range Constraints

These constraints represent ranges of values a particular value number can take at a given point in the program. As we saw previously these ranges can be discrete values, or true continuous (potentially discrete) ranges. Range constraints can be used to eliminate control flow or propagate particular values to different points in the program. There are many range constraint classes for tracking different types, such as integers and longs.

### Type Constraints

For typed languages it is often beneficial to flow type constraints. Knowing type information about a particular value number at a given point in the program can allow us to devirtualize calls or eliminate type check and exception edges.

There are three different constraint classes which flow this information in VP, they are the [fixed class](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/VPConstraint.hpp#L655-L674) constraint, [resolved class](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/VPConstraint.hpp#L628-L653) constraint, and [unresolved class](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/VPConstraint.hpp#L706-L736) constraints.

Fixed class constraints simply express that a value number has a specific, fixed, type `T`. We can know this information perhaps because we have seen the allocation of the value number expression or perhaps we have seen an explicit cast into type `T` where `T` is a `final` type (meaning it cannot be subtyped). Knowing the exact type of a value number expression is the most useful.

Resolved class constraints express that a value number is of type `T` or any subclass of `T`. These constraints are useful for eliminating additional type checks on the value number at some later point in the program.

Unresolved class constraints are least interesting and express that the class type of the value number has not been loaded by the runtime and is unknown. They simply carry around the string representing the class of the object.

### Null and NonNull Constraints

There are two notions flowed through VP for reference types in regards to nullness. The [null](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/VPConstraint.hpp#L746-L760) constraint, and the [non-null](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/VPConstraint.hpp#L762-L775) constraint. These constraints are useful for tracking value numbers which are certainly known to be either `null` or certainly not `null`. Knowing such information can eliminate implicit and explicit null checks or open doors for further simplifications, such as folding away of comparisons (`ifacmpeq/ne`) that test against `null`.

Note that although these constraint classes exist, we typically query them through the `VPClass` base class constraint. It is usually more convenient and efficient to store all the various information about a reference type in this base class, rather than storing multiple individual constraints for type, nullness, etc.

### Absolute vs. Relative Constraints

Absolute constraints are explicit constraints about a particular value number. For example a type constraint, a range constraint, a null constraint are all examples or absolute constraints.

Relative constraints are constraints that connect one value number to another value number. For example if we were to encounter a guarded comparison between `x` and `y`, say `x < y` then we would create a relative constraint between the value numbers of `x` and `y` to specify that on that path the value number for `x` is less than the value number for `y`.

Relative constraints are useful when neither `x` nor `y` are known to be constant, and we still want to propagate information about the relation of their value numbers. This is useful if for example we encounter another condition which is a successor of the previous check which asked if `x < y + 5`. We want to be able to fold away such expressions even though we don't know any absolute constraints about the value numbers of `x` nor `y`. Sometimes overflow possibilities can mean that we cannot fold away compares such as the above but the general idea is conveyed.

## Local vs. Global Value Propagation

There are two modes in which the value propagation optimization can run; local and global. Both modes run using the same constraints and VP handlers. What is different is the scope at which they operate.

Local value propagation processes the program one block at a time. If block extension has ran it processes the program one extended block at a time. Information is not propagated between blocks, so any information we learned is discarded after arriving at the end of the (extended) basic block. This of course makes the local value propagation optimization restrictive in what it can accomplish.

Because local value propagation runs at this reduced scope there is no need to calculate value numbers for all the nodes in the program. We can simply use the global index of the node as the value number, since if local value propagation runs after local common subexpression elimination (CSE), the nodes within a block will have already been commened up and they will have the same global index. Retrieving the global index is very cheap, and in addition other expensive metadata such as use-def info need not be computed to run local value propagation.

This makes it the perfect candidate to run frequently and at low optimization levels.

Global value propagation on the other hand is a global optimization which requires both global value numbering and use-def information to be computed. In the context of the OMR compiler global means at the scope of the current compilation unit, not inter procedural. The extra metadata information which needs to be computed is relatively expensive (in terms of memory footprint and compile time) and this is why the global value propagation optimization is only run at higher optimization levels (warm and above).

In addition to requiring extra metadata to be computed, global value propagation is a memory intensive operation as it requires us to intersect and merge constraints at control flow points in the program. This can mean having to propagate hundreds if not thousands of constraints through the lifetime of the optimization.

It is important to note much of the infrastructure is shared among the local and global variants of the optimization and that we seldom encounter the check for local vs. global in the shared codebase, although such checks do exist, for example for retrieving the value number info for a particular node, among other locations.

### Handling Loops

During value propagation blocks outside a loop are typically analyzed once while blocks inside a loop are analyzed twice. The reason why blocks inside of loops are analyzed twice is because value propagation processes information by analyzing one node at a time using a depth first approach. This means we analyze the children recursively before analyzing the parent. Within a loop we cannot make a decision about a transformation until we have reached the back edge of the loop because we may not have seen all definitions (defs) of a particular value (because we haven't analyzed it yet). Once we reach the back edge we propagate constraints through the back edge and analyze the loop body a second time.

As such there is the `lastTimeThrough` API which is sometimes queried to determine whether this is the last (second) time we will be processing a particular block.

### Store Constraints

There is something special to be said about constraints on stores in the context of global value propagation. In value propagation, the absence of a constraint for a particular value number implies the absence of information about that value number. That is, if we don't know anything about a particular value number we don't generate and store a constraint which says "I don't know anything about the range of values this value number can take". This is done for memory efficiency when intersecting and merging constraint values during propagation.

However consider the following simple while loop:

```
i = 0;

while (i < 10) {
    print(i);
    i = i + 1;
}
```

While analyzing this program the value propagation optimization will have created a constraint for the value number of `i` to have a constant constraint of `0`. Later when it encounters the load of `i` as an argument to the `print` function call we need to ensure that we don't fold away the load with the constraint on the value number of `i` which is `0` at that point. This is because we haven't yet processed the second store to `i` which is the loop increment.

To accomplish this the value propagation optimization will ask for the defs (stores which can reach this particular load) of `i` and it will loop through all the defs and check whether they have a constraint. If we encounter a def  which doesn't have a constraint it implies we are in a loop and we haven't yet processed and created all the constraints for `i` and as such we cannot replace the load of `i` with `0` at this point in the analysis.

The absence of a constraint on a store actually means something in this context, which is contrary to what we explained above. It is indeed a special case. Once the analysis reaches the second store, it will update the constraint on the value number for `i` and the second pass through the loop will query whether all the defs for the load of `i` in the `print` function call have a constraint, in which case the answer will be true, and since we have seen the second store the value number of `i` will no longer have a constant constraint of `0`, meaning that we cannot constant fold away the load of `i`.

### Global Constraint Lists

The notion of a global constraint is one which doesn't change for the duration of hte analysis. A typical example of this can be explained by the following program:

```
i = 5

if (...) {
    ...
} else {
    ...
}
```

The value number of the right hand side of the statement `i = 5` has a constraint which cannot be changed. So why bother to propagate such a constraint through the successor edges of the subsequent `if` statement. Rather than doing this we keep track of a global constraint list to which we add such global constraints which are valid throughout the entire analysis. This saves on memory footprint and time to copy the constraints around as the analysis flows through the method.


## Delayed Transformations

For some optimizations, the context information available during value propagation makes it a very enticing place to perform a particular transformation. A typical example of this is reducing a call into an intrinsic IL, for example the `arraycopy` IL.

Because value propagation relies on all nodes having valid value numbers it is impossible for us to perform a transformation which creates a new node out of thin air during the analysis. This is because for loop back edges for example we may visit the same node twice, and we could potentially encounter a node without a value number. We could of course create a unique value number for this new node at the point of creation, but for one reason or another we chose to handle such operations by employing the notion of _delayed transformations_.

During value propagation if we encounter a situation where we would like to create a brand new node, what we do is we record this node into a list for later processing. Once the the value propagation analysis is done we no longer have a need for the value numbers, and we can safely process all the recorded nodes. This operation is done by the [doDelayedTransformations](https://github.com/eclipse/omr/blob/25f479d2b671c2a0a03156cc0a48024a46d2e3c2/compiler/optimizer/OMRValuePropagation.hpp#L467) API.

Note that if we want to transmute node `A` into node `B` we can still do so during the analysis phase as long as both node `A` and node `B` have valid value numbers. Similarly we can eliminate nodes from the program by simply removing them (for example eliminating `null` checks).

## Handling Exception Edges

In OMR exception edges do not break up basic blocks, instead they are handled via what are known as _check nodes_. This was a decision made very early in the development of the compiler component so as to avoid having control flow graphs with many tiny blocks which would hinder the effectiveness of local optimizations.

These check nodes can be quite common in languages such as Java where we would have nodes representing null checks (`NULLCHK`), bound checks (`BNDCHK`), etc. All of these check nodes can cause an exception to occur (`NullPointerException` and `ArrayIndexOutOfBoundsException` respectively). In the compiler we create exception edges from blocks containing such excepting nodes to their corresponding catch blocks.

Optimizations such as value propagation which walk the control flow graph need to deal with such exception edges. The reason for this is because execution can resume if the catch block handles the exception and falls through to the next block. We need to properly keep track of the constraints at the merge point of the catch block and the fall-through of the try block.

To do this the value propagation optimization merges the set of constraints known at the time we are examining a node which can cause an exception onto the exception edge. Each new node which can cause an exception (check nodes included, as well as things like calls, etc.) that we encounter we perform another merge operation because the set of constraints may have changed between two nodes which can cause an exception and could be several tree tops apart in the _same_ block.
