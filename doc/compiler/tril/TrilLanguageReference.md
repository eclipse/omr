<!--
Copyright (c) 2017, 2017 IBM Corp. and others

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

# Tril Language Reference

In many ways, the Tril language mirrors the Testarossa Intermediate Language,
_Trees_. 

This language reference intends to document the places where special behaviour
has been created in Tril to make writing test cases easier. 

# Basics

Tril is built out of two main components: 

1. A very permissive S-expression parser.
2. An IlGenerator that walks the AST produced by said parser, and generates
   Trees. 

# Syntax

Tril represents Trees using S-expressions, with key-value properties to augment
a particular node with more information. 

* `(x (y) (z))` represents some `x` with `y` and `z` as children.
* `(x foo=bar baz=braz (y) (z)` is the same tree as above, but where the node
  for `x` is augmented with two properties with values. 

  Anything may be provided as a property for a node, however, unknown
  properties will be silently ignored.

## Literals

* Signed integer literals can be input directly, and will always be treated internally
  as a 64-bit integer.  
* Hexadecimal integer literals are supported as well, and are treated as
  64-bit integers. Use a leading `0x`. 
* Floating point numbers can be input by including a decimal point.  

## Comments 

`;` starts a comment in Tril, and the comment proceeds to the end of a line. 

## Special Forms: 

The key to Tril is the unlocking of extra behaviour using 'special forms', or
atoms that represent certain behaviours in Testarossa. 

### Types

The Testarossa IL, and therefore Tril, deals with the following data types. 

* `Int8`
* `Int16`
* `Int32`
* `Int64`
* `Address`
* `Float`
* `Double`
* `VectorInt8`
* `VectorInt16`
* `VectorInt32`
* `VectorInt64`
* `VectorFloat`
* `VectorDouble`
* `NoType`

Signedness is a property of operations, not the data itself.

### Methods

A whole method in Tril is wrapped in a `method` expression. 

#### Properties 

* `return` _Mandatory_ is the return type of the method. 
* `args` _Optional_ is a square-bracketed list of arguments representing the
  types of the parameters, and the order in which they appear. 

  eg. `args=[Int32,Double,Float]`
    
### Block

In the Testarossa IL, all trees exist within the context of a basic block. In 
Tril, blocks must be children of a `method`, or they will not be recognized.

#### Properties

* `name` _Optional_ Blocks can be named in order to target them with branches. 

### Stores and loads

Currently the creation of new symbol references in Tril is not supported.
However, store and load opcodes provide two options for where the store / load
can come from, which can be specified with one of the following two properties: 

* `parm` can specify the parameter from which a load can come. Parameters are 
  counted from zero.  eg. `(iload parm=2)` loads the third parameter from the
  signature. 
* `temp` Stores to temps create new temporary variables that can later be
  referenced. eg. `(istore temp="x" ...)`

### Branches 

Branches evaluate a condition then either jump to a target, or falls through to
the next block. The fallthrough block can be eiher implied, in which case it
will be the next block textually, or it can be specified manually with the 
`fallthrough` property. 

#### Properties

* `target` _Mandatory_ The `name` of the block a successful computation of the
  condition will continue executing in. 
* `fallthrough` _Optional_ In some circumstances it is necessary to explicitly
  list the `name` of the fallthrough block of a branch to ensure the desired CFG is
  constructed. In addition to a block `name`, there are two special values that
  may be used here: 
   * `@none` indicates no fallthrough edge need be generated: this can be the
     case for certain opcodes, ie

            (goto target="unconditionalTarget" fallthrough=@none)

   * `@exit` falls through to the exit block of the CFG. This can be useful for
     constructing invalid control flow in the case of test-cases.

#### Example

```
(method ...
   (block fallthrough="sub" name="start"
      (ificmpge target="add"                 
        ...) )
   (block name="sub"
      (ireturn
         ...) )
   (block name="add"
      (ireturn                               
        ... ) )
```
   
### Commoning 

Nodes can be given identifiers to allow creating commoned subtrees. To common a
reference, on first computation annotate the node with the `id="<some name>"`
property. Then, where commoning is desired, replace the node with `(@id "<some
name>")` 

```
(imul id="multiplyResult"
    ...) 
(iadd
  (@id "multiplyResult")
  (@id "multiplyResult")
```

### Vectors 

Vector support in Tril is very preliminary, but present. One of the most
important pieces is the ability to annotate indirect vector loads and stores
with the data type of the load. 

This is required because the vector support in Testarossa infers the vector
types from certain roots, as opposed to expressing the data types in the
opcodes directly like other opcodes in Testarossa. 

    (vloadi type=VectorDouble (aload parm=0))
    
####Properties

* `type` _Mandatory_ The data type being loaded or stored. 



### Calls

Call opcodes are supported with in Tril by providing extra annotation of the
argument types and the function pointer to be invoked.

    (icall address=0x1234 args=[Int32] ...) 

####Properties

* `address` _Mandatory_ The address of the function to be invoked by the call opcode
* `args` _Optional_ The list of argument types passed to the call. 


# Examples 

## Incordec 

Checked in as `fvtest/tril/examples/incordec/incordec.tril`

```
; This simple method takes as argument a pointer to a 32-bit integer. If the
; integer pointed to by the argument has a negative value, then the value
; decremented by 1 is returned. Otherwise (value is positive), the value
; incremented by 1 is returned.
;
; An equivalent C implementation:
;
; int incordec(int* parm0) {
;    if (*parm0 < 0) {
;     return *parm0 - (int)1.0;
;    } else { 
;      int x = 1;
;      return *parm0 + x;
;    }
; }

(method name="incordec" return="Int32" args=["Address"]
   (block fallthrough="sub" name="start"     ; start:
      (ificmpge target="add"                 ; if (*parm0 >= 0) goto add;
         (iloadi offset=0 (aload parm=0) )
         (iconst 0) ) )
   (block name="sub"                         ; sub:
      (ireturn                               ; return *parm0 - 1;
         (isub
            (iloadi offset=0 (aload parm=0) )
            (d2i (dconst 1.0) ) ) ) )
   (block name="add"                         ; add:
      (istore temp="x" (iconst 1))           ; int x = 1;
      (ireturn                               ; return *parm0 + x;
         (iadd
            (iloadi offset=0 (aload parm=0) )
            (iload temp="x") ) ) ) )
```

## Vector Support: 

Vector addition of two input double vectors into a third output double vector.
Hooked up as a test case in `fvtest/compilertriltest/VectorTest.cpp`.

```
(method return= NoType args=[Address,Address,Address]          
  (block                                                       
     (vstorei type=VectorDouble offset=0                       
         (aload parm=0)                                        
            (vadd                                              
                 (vloadi type=VectorDouble (aload parm=1))     
                 (vloadi type=VectorDouble (aload parm=2))))   
     (return)))                                                
```
