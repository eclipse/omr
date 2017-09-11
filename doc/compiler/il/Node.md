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

# Node

## Description

When an optimizer makes a transformation, a Node is often transmuted from one opcode value to another. This can lead to a variety of bugs since the properties that belonged to the original Node are implicitly inherited by the transmuted Node. The old properties might have no meaning, or worse, a different meaning for the new transmuted Node.

To fix this problem, a Node is immutable, in that its opcode value cannot be modified once the Node has been created. Any transformation that requires a change in the Node's opcode value requires the allocation of a new Node.

This technique can also be applied to other properties of Nodes that should be considered immutable. For example, the `intValue` on an `iconst` Node.

## Design Details

### Immutable Properties of a Node

Under an optimizer, we are allowed to transmute an expression as long as its "abstract value", or meaning,  remains unchanged. To keep the expression's abstract value unchanged, we may need to change several things at once. In most cases, just changing the opcode alone will change the abstract value of the expression. For example, consider the transmutation (for positive integer `x`):

	x * 2	---->	x << 1

This is represented as:
```
   *                   <<
 /   \     ---->     /    \
x     2             x      1
```

The expressions above have the same abstract value because they mean the same thing. The transmutation shown is valid, but this is only true because several things happen at once: 

* The `*` becomes a `<<`
* The `2` is replaced by a `1`

In general, there may be lots of things that have to happen at once during a transmutation: Multiple properties of the Node and of the children might have to be changed. We have to consider the whole expression not just the top level Node here. Just changing the opcode value of the Node is not enough. In the example above, if the `*` is transmuted to a `<<` and its children remain the same, then the resulting expression is incorrect. `x * 2` does not mean the same as `x << 2`. We wish to avoid this temporary inconsistency since it is prone to bugs. The answer is to disallow intermediate (and incorrect) transmutations by not allowing just the Node's opcode value to be altered. Instead, the Node expression must be recreated, so the transmutation of the expression is done atomically.

### Transmutating a Node Expression Inplace

If we were to create a new Node to replace an old one, we would normally need to update all of the references to the old Node. For instance, suppose in the transformation

	x * (2 ** n)  ---->  x << n

we wanted to to replace `imul` by `ishl`, then we would normally need to update all of the references of the original `imul` Node (the expression of which may be commoned) to refer to the new `ishl` Node. Finding all these places can be expensive.

But with the introduction of the NodePool we can avoid this expense; we can allocate a new Node over the old one (any Node is the same size as any other), such that its address (ie. index in the Node pool) is unchanged. We do not need to find and update any references, and therefore this allows us to implement immutable Nodes with little overhead.

Normally we would create a Node like this:

```cpp
TR::Node *Node = TR::Node::create(...);
```

and `create` would create a new Node from the NodePool like this:

```cpp
TR::Node *Node = new (comp->getNodePool()) TR::Node(...);
```

where the Node pool allocator would add a new Node entry to the NodePool table, and return a reference to that entry.

There also exists an alternative Node interface; a new Node can be created over the original Node using `recreateWithoutProperties`:

```cpp
TR::Node *recreatedNode = TR::Node::recreateWithoutProperties(originalNode, ...);
```

The returned value (the address of the recreated Node) should be identical to the address of the original Node passed to recreate. The old Node can be recreated like this (we may also need to "destroy" originalNode without deallocating its memory from the Node pool):

```cpp
originalIndex = originalNode->getNodePoolIndex();
TR::Node *Node = new (comp->getNodePool(), originalIndex) TR::Node(...);
```

Here, we provide an alternate form of the `new` operator. It has an extra parameter: the index of the original Node that we wish to allocate on top of. In this case, instead of adding an entry to the table, the allocator just checks the index and returns the original storage, which is cleared and initialized normally.

When the immutable recreate interface is used, the properties of the new Node need to be explicitly set. Anything not explicitly set in implicitly unset. This will lead to code that is more explicit about what it means to do.

### Recreate functions that do not copy properties

When the replacement Node bears no or little relationship to the original Node, little or no properties should be copied to the new Node. Thus some recreate functions do not copy any properties, children or symRefs, but they do recreate the Node with new children and new symRefs. These are:

```cpp
static TR::Node *recreateWithoutProperties(TR::Node *originalNode, OMR::ILOpCodes op, uint16_t numChildren, [TR::Node *first, ... ]);
static TR::Node *recreateWithSymRefWithoutProperties(TR::Node *originalNode, OMR::ILOpCodes op, uint16_t numChildren, uint16_t numChildArgs, TR::Node *first, ChildrenAndSymRefType... childrenAndSymRef);
```

### Recreate functions that do copy properties

In practice, the above type of mutation is rare. In most cases the replacement Node has a close relationship to the original Node. Thus the more common interface is this:

```cpp
static TR::Node *recreate(TR::Node *originalNode, OMR::ILOpCodes op);
```

The function `recreate` recreates a replacement Node with the new opcode, but with all the original properties, children, and symRefs as the original as long as they may be valid for the new opcode. Unlike `setOpCodeValue`, `recreate` will clear invalid properties.
